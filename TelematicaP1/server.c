// server.c — Servidor CoAP completo con CON/NON/ACK/RST y métodos GET/POST/PUT/DELETE.
// Sockets Berkeley (UDP), almacenamiento en memoria por recurso y logger a consola + archivo.

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef int socklen_t;
#else
  #include <arpa/inet.h>
  #include <sys/socket.h>
  #include <unistd.h>
  #include <pthread.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>   // <- necesario para log_line

/* ======== CoAP: tipos y códigos ======== */
#define COAP_VER         1
#define COAP_TYPE_CON    0
#define COAP_TYPE_NON    1
#define COAP_TYPE_ACK    2
#define COAP_TYPE_RST    3

#define COAP_GET         1
#define COAP_POST        2
#define COAP_PUT         3
#define COAP_DELETE      4

#define COAP_2_01_CREATED 0x41
#define COAP_2_04_CHANGED 0x44
#define COAP_2_05_CONTENT 0x45
#define COAP_4_00_BADREQ  0x80
#define COAP_4_04_NOTFND  0x84
#define COAP_5_00_SRVERR  0xA0

/* ======== Opciones CoAP ======== */
#define OPT_URI_PATH       11
#define OPT_CONTENT_FORMAT 12
#define CF_APP_JSON        50  // application/json

/* ======== Logger sencillo (consola + archivo) ======== */
static FILE *glog = NULL;
static void ts_now(char *buf, size_t n){
  time_t t = time(NULL);
  struct tm tm;
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tm);
}
static void log_line(const char *fmt, ...){
  char tbuf[32]; ts_now(tbuf, sizeof tbuf);
  va_list ap;
  /* consola */
  printf("[%s] ", tbuf);
  va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
  printf("\n"); fflush(stdout);
  /* archivo */
  if(glog){
    fprintf(glog, "[%s] ", tbuf);
    va_start(ap, fmt); vfprintf(glog, fmt, ap); va_end(ap);
    fprintf(glog, "\n"); fflush(glog);
  }
}

/* ======== Almacenamiento en memoria por recurso ======== */
#define MAX_ITEMS 32
#define KEY_MAX   128
#define VAL_MAX   1024
typedef struct { int used; char key[KEY_MAX]; char val[VAL_MAX]; } kv_t;
static kv_t tab[MAX_ITEMS];

static int find_slot(const char *key){
  for(int i=0;i<MAX_ITEMS;i++)
    if(tab[i].used && strcmp(tab[i].key,key)==0) return i;
  return -1;
}
static int find_free(void){
  for(int i=0;i<MAX_ITEMS;i++)
    if(!tab[i].used) return i;
  return -1;
}
static int store_upsert(const char *key, const char *json){
  int i = find_slot(key);
  if(i<0){ i=find_free(); if(i<0) return -1; }
  snprintf(tab[i].key, KEY_MAX, "%s", key);
  snprintf(tab[i].val, VAL_MAX, "%s", json);
  tab[i].used = 1;
  return 0;
}
static int store_get(const char *key, char *out, int outsz){
  int i = find_slot(key);
  if(i<0) return -1;
  snprintf(out, outsz, "%s", tab[i].val);
  return 0;
}
static int store_delete(const char *key){
  int i = find_slot(key);
  if(i<0) return -1;
  tab[i].used=0; tab[i].key[0]=0; tab[i].val[0]=0;
  return 0;
}

/* ======== Construcción de respuestas CoAP ======== */
/* Nota: añadimos Content-Format: application/json (50) cuando code == 2.05 */
static size_t coap_build_msg(uint8_t *out, size_t cap,
                             uint8_t type, uint8_t code, uint16_t mid,
                             const uint8_t *token, uint8_t tkl,
                             const char *payload){
  size_t pos=0;
  if(cap<4) return 0;
  out[pos++] = ((COAP_VER&3)<<6) | ((type&3)<<4) | (tkl&0x0F);
  out[pos++] = code;
  out[pos++] = (mid>>8)&0xFF; out[pos++] = mid&0xFF;
  if(tkl){
    if(pos+tkl>cap) return 0;
    memcpy(&out[pos], token, tkl); pos+=tkl;
  }
  /* Si vamos a devolver contenido, añade opción Content-Format (n=12, len=1, val=50).
     Como no hemos enviado opciones antes, last=0 y delta=12 (<13) → byte 0xC1 seguido de 0x32 (50). */
  if(code == COAP_2_05_CONTENT){
    if(pos+2>cap) return 0;
    out[pos++] = (OPT_CONTENT_FORMAT<<4) | 1; // 0xC1
    out[pos++] = CF_APP_JSON;                 // 50
  }
  if(payload && payload[0]){
    size_t plen = strlen(payload);
    if(pos+1+plen>cap) return 0;
    out[pos++] = 0xFF;
    memcpy(&out[pos], payload, plen); pos+=plen;
  }
  return pos;
}

static void send_coap_reply(
#ifdef _WIN32
  SOCKET s,
#else
  int s,
#endif
  struct sockaddr_in *cli, socklen_t cl,
  uint8_t resp_type, uint8_t code, uint16_t mid,
  const uint8_t *token, uint8_t tkl, const char *payload)
{
  uint8_t out[1500];
  size_t m = coap_build_msg(out,sizeof out, resp_type, code, mid, token, tkl, payload);
  if(!m) return;
#ifdef _WIN32
  sendto(s,(const char*)out,(int)m,0,(struct sockaddr*)cli,cl);
#else
  sendto(s,out,m,0,(struct sockaddr*)cli,cl);
#endif
}

static void send_rst(
#ifdef _WIN32
  SOCKET s,
#else
  int s,
#endif
  struct sockaddr_in *cli, socklen_t cl, uint16_t mid)
{
  uint8_t out[4];
  out[0] = ((COAP_VER&3)<<6) | ((COAP_TYPE_RST&3)<<4) | 0; // TKL=0
  out[1] = 0;  // Code=0
  out[2] = (mid>>8)&0xFF; out[3] = mid&0xFF;
#ifdef _WIN32
  sendto(s,(const char*)out,4,0,(struct sockaddr*)cli,cl);
#else
  sendto(s,out,4,0,(struct sockaddr*)cli,cl);
#endif
}

/* ======== Parse de opciones para extraer URI-Path ======== */
typedef struct { uint16_t num; const uint8_t *val; uint16_t len; } opt_t;

static int parse_options(const uint8_t *buf, int len, int tkl,
                         opt_t *opts, int *optc,
                         const uint8_t **payload, int *plen){
  int pos = 4 + tkl;
  uint16_t last = 0; int count = 0;
  *payload = NULL; *plen = 0;
  if(pos>len) return -1;
  while(pos < len){
    if(buf[pos]==0xFF){ pos++; *payload=&buf[pos]; *plen = len-pos; return 0; }
    if(pos>=len) break;
    uint8_t b = buf[pos++];
    uint16_t d = (b>>4)&0xF, l = b&0xF;
    if(d==13){ if(pos>=len) return -1; d = 13 + buf[pos++]; }
    else if(d==14){ if(pos+1>=len) return -1; d = 269 + ((buf[pos]<<8)|buf[pos+1]); pos+=2; }
    if(l==13){ if(pos>=len) return -1; l = 13 + buf[pos++]; }
    else if(l==14){ if(pos+1>=len) return -1; l = 269 + ((buf[pos]<<8)|buf[pos+1]); pos+=2; }
    uint16_t num = last + d; last = num;
    if(pos + l > len) return -1;
    if(count<16){ opts[count].num=num; opts[count].val=&buf[pos]; opts[count].len=l; count++; }
    pos += l;
  }
  *optc = count;
  return 0;
}

static void build_path(char *out, int outsz, const opt_t *opts, int optc){
  int w=0; out[0]=0;
  for(int i=0;i<optc;i++){
    if(opts[i].num==OPT_URI_PATH){
      if(w+1<outsz) out[w++]='/';
      for(int k=0;k<opts[i].len && w+1<outsz;k++) out[w++] = (char)opts[i].val[k];
    }
  }
  if(w==0){ if(outsz>1){ out[0]='/'; out[1]=0; } }
  else out[w]=0;
}

/* ======== Job por petición (para POSIX threads) ======== */
typedef struct {
#ifdef _WIN32
  SOCKET s;
#else
  int s;
#endif
  struct sockaddr_in cli; socklen_t cl;
  int n; uint8_t buf[1500];
} job_t;

static void handle_one(
#ifdef _WIN32
  SOCKET s,
#else
  int s,
#endif
  struct sockaddr_in *cli, socklen_t cl, const uint8_t *buf, int n)
{
  if(n<4) return;

  uint8_t ver = (buf[0]>>6)&3;
  uint8_t req_type = (buf[0]>>4)&3; // CON/NON esperado
  uint8_t tkl = buf[0]&0x0F;
  uint8_t code = buf[1];
  uint16_t mid = ((uint16_t)buf[2]<<8)|buf[3];

  if(ver!=COAP_VER || tkl>8){
    log_line("MSG inválido: ver=%u tkl=%u -> RST", ver, tkl);
    send_rst(s,cli,cl,mid);
    return;
  }

  uint8_t token[8]; memset(token,0,8);
  if(tkl){ if(4+tkl>n){ send_rst(s,cli,cl,mid); return; } memcpy(token, buf+4, tkl); }

  opt_t opts[16]; int optc=0; const uint8_t *payload=NULL; int plen=0;
  if(parse_options(buf,n,tkl,opts,&optc,&payload,&plen)<0){ send_rst(s,cli,cl,mid); return; }

  char path[KEY_MAX]; build_path(path,sizeof path,opts,optc);

  /* Fallback: si no llegó URI-Path, usar /sensor por defecto */
  if (path[0]=='/' && path[1]=='\0') {
    log_line("URI-Path vacío -> usando /sensor por defecto");
    snprintf(path, sizeof path, "/sensor");
  }

  char body[VAL_MAX]="";
  if(payload && plen>0){
    if(plen >= (int)sizeof(body)) plen=(int)sizeof(body)-1;
    memcpy(body,payload,(size_t)plen); body[plen]=0;
  }

  log_line("REQ type=%s code=0x%02X mid=0x%04X path=%s plen=%d",
           (req_type==COAP_TYPE_CON?"CON": req_type==COAP_TYPE_NON?"NON":"UNK"),
           code, mid, path, plen);
  if(plen>0) log_line("BODY: %s", body);

  /* Elegir tipo de respuesta: ACK si CON, NON si NON */
  uint8_t resp_type = (req_type==COAP_TYPE_CON) ? COAP_TYPE_ACK : COAP_TYPE_NON;
  char resp[VAL_MAX]=""; uint8_t code_resp = COAP_4_00_BADREQ;

  if(code==COAP_GET){
    if(store_get(path, resp, (int)sizeof resp)==0){
      code_resp = COAP_2_05_CONTENT;
      log_line("GET %s -> %s", path, resp);
    }else{
      code_resp = COAP_4_04_NOTFND;
      snprintf(resp,sizeof resp,"err");
      log_line("GET %s -> not found", path);
    }
  }else if(code==COAP_POST || code==COAP_PUT){
    if(body[0]){
      store_upsert(path, body);
      code_resp = COAP_2_04_CHANGED;
      snprintf(resp,sizeof resp,(code==COAP_POST)?"stored":"updated");
      log_line("%s %s -> OK", (code==COAP_POST?"POST":"PUT"), path);
    }else{
      code_resp = COAP_4_00_BADREQ;
      snprintf(resp,sizeof resp,"bad");
      log_line("%s %s -> body vacío", (code==COAP_POST?"POST":"PUT"), path);
    }
  }else if(code==COAP_DELETE){
    if(store_delete(path)==0){
      code_resp = COAP_2_04_CHANGED;
      snprintf(resp,sizeof resp,"deleted");
      log_line("DELETE %s -> OK", path);
    }else{
      code_resp = COAP_4_04_NOTFND;
      snprintf(resp,sizeof resp,"err");
      log_line("DELETE %s -> not found", path);
    }
  }else{
    code_resp = COAP_4_00_BADREQ;
    snprintf(resp,sizeof resp,"err");
    log_line("Método no soportado: 0x%02X", code);
  }

  send_coap_reply(s, cli, cl, resp_type, code_resp, mid, token, tkl, resp);
}

#ifndef _WIN32
static void* worker(void *arg){
  job_t *j = (job_t*)arg;
  handle_one(j->s, &j->cli, j->cl, j->buf, j->n);
  free(j);
  return NULL;
}
#endif

int main(int argc, char **argv){
  if(argc<3){
    fprintf(stderr, "Uso: %s <PORT> <LogFile>\n", argv[0]);
    return 1;
  }
  int port = atoi(argv[1]);
  /* abrir log */
  glog = fopen(argv[2], "a");

#ifdef _WIN32
  WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
  SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
  if(s==INVALID_SOCKET){ fprintf(stderr,"socket() fail\n"); return 1; }
#else
  int s = socket(AF_INET, SOCK_DGRAM, 0);
  if(s<0){ perror("socket"); return 1; }
#endif

  struct sockaddr_in srv; memset(&srv,0,sizeof srv);
  srv.sin_family = AF_INET;
  srv.sin_addr.s_addr = htonl(0);
  srv.sin_port = htons(port);

#ifdef _WIN32
  if(bind(s,(struct sockaddr*)&srv,sizeof srv)==SOCKET_ERROR){ fprintf(stderr,"bind() fail\n"); return 1; }
#else
  if(bind(s,(struct sockaddr*)&srv,sizeof srv)<0){ perror("bind"); return 1; }
#endif

  log_line("Servidor CoAP escuchando en UDP %d", port);

  for(;;){
    job_t *j = (job_t*)malloc(sizeof *j);
    if(!j){ log_line("malloc() fail"); break; }
    j->s=s; j->cl=(socklen_t)sizeof j->cli;

#ifdef _WIN32
    j->n = recvfrom(s, (char*)j->buf, (int)sizeof j->buf, 0, (struct sockaddr*)&j->cli, &j->cl);
    if(j->n==SOCKET_ERROR){ free(j); continue; }
    /* En Windows: manejamos inline (sin pthread) */
    handle_one(j->s, &j->cli, j->cl, j->buf, j->n);
    free(j);
#else
    j->n = (int)recvfrom(s, j->buf, sizeof j->buf, 0, (struct sockaddr*)&j->cli, &j->cl);
    if(j->n<=0){ free(j); continue; }
    pthread_t th;
    if(pthread_create(&th,NULL,worker,j)!=0){ log_line("pthread_create fail"); free(j); continue; }
    pthread_detach(th);
#endif
  }

  if(glog){ fclose(glog); glog=NULL; }
#ifdef _WIN32
  closesocket(s); WSACleanup();
#else
  close(s);
#endif
  return 0;
}
