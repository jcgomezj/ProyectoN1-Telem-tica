#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef int socklen_t;
#else
  #include <arpa/inet.h>
  #include <sys/socket.h>
  #include <unistd.h>
#endif
#include <pthread.h>

#include "coap_min.h"
#include "logger.h"
#include "store.h"

struct job {
#ifdef _WIN32
  SOCKET s;
#else
  int s;
#endif
  struct sockaddr_in cli; socklen_t cl;
  size_t n; uint8_t buf[1500];
};

static void sendto_wrap(
#ifdef _WIN32
  SOCKET s,
#else
  int s,
#endif
  const uint8_t* out, int m, struct sockaddr_in *cli, socklen_t cl) {
#ifdef _WIN32
  sendto(s,(const char*)out,(int)m,0,(struct sockaddr*)cli,cl);
#else
  sendto(s,out,m,0,(struct sockaddr*)cli,cl);
#endif
}

static void* worker(void *arg){
  struct job *j=(struct job*)arg;
  coap_msg_t req; if(coap_parse(j->buf,j->n,&req)!=0){ log_line("[WARN] bad packet"); goto out; }

  // Uri-Path -> construir string "/a/b"
  char path[256]; size_t used=0; path[0]='\0';
  for(size_t i=0;i<req.optc;i++) if(req.opt[i].num==OPT_URI_PATH){
    if(used+1>=sizeof path) break; path[used++]='/';
    size_t n=req.opt[i].len; if(used+n>=sizeof path) n=sizeof(path)-used-1;
    memcpy(&path[used], req.opt[i].val, n); used+=n; path[used]='\0';
  }
  if(used==0) { strcpy(path,"/"); }

  // Content-Format (si llega)
  int cf=-1; for(size_t i=0;i<req.optc;i++) if(req.opt[i].num==OPT_CONTENT_FORMAT){
    int v=0; for(size_t k=0;k<req.opt[i].len;k++) v=(v<<8)|req.opt[i].val[k]; cf=v;
  }

  log_line("[REQ] %s code=0x%02X mid=0x%04X path=%s",
           "udp", req.h.code, req.h.mid, path);

  uint8_t out[1500]; size_t mlen=0; const char *payload=NULL; uint8_t code=COAP_4_00_BADREQ;

  // Rutas:
  // 1) /ping -> GET -> "pong"
  if(strcmp(path,"/ping")==0 && req.h.code==COAP_GET){
    code=COAP_2_05_CONTENT; payload="pong";
  }
  // 2) /sensors/<id> -> GET/POST/PUT/DELETE con JSON
  else if(strncmp(path,"/sensors/",9)==0){
    const char *sid=path+9;
    if(req.h.code==COAP_GET){
      static char buf[1024];
      if(store_get(sid,buf,sizeof buf)==0){ code=COAP_2_05_CONTENT; payload=buf; }
      else { code=COAP_4_04_NOTFND; payload=NULL; }
    } else if(req.h.code==COAP_POST || req.h.code==COAP_PUT){
      if(cf==CF_APP_JSON && req.payload && req.plen>0){
        char *tmp=(char*)malloc(req.plen+1); memcpy(tmp,req.payload,req.plen); tmp[req.plen]='\0';
        store_upsert(sid,tmp);
        free(tmp);
        code = (req.h.code==COAP_POST)? COAP_2_01_CREATED : COAP_2_04_CHANGED;
      } else {
        code=COAP_4_00_BADREQ;
      }
    } else if(req.h.code==COAP_DELETE){
      if(store_delete(sid)==0) code=COAP_2_04_CHANGED; else code=COAP_4_04_NOTFND;
    } else {
      code=COAP_4_00_BADREQ;
    }
  }

  mlen = coap_build_reply(out,sizeof out, COAP_TYPE_ACK, code, req.h.mid, req.h.token, req.h.tkl, payload);
  if(mlen>0) sendto_wrap(j->s,out,(int)mlen,&j->cli,j->cl);
  log_line("[RES] code=0x%02X mid=0x%04X len=%zu", code, req.h.mid, mlen);

out:
  free(j); return NULL;
}

int main(int argc, char **argv){
  if(argc!=3){ fprintf(stderr,"Uso: %s <PORT> <LogFile>\n", argv[0]); return 1; }
  int port=atoi(argv[1]); logger_init(argv[2]);

#ifdef _WIN32
  WSADATA wsa; if(WSAStartup(MAKEWORD(2,2), &wsa)!=0){ fprintf(stderr,"WSAStartup failed\n"); return 1; }
  SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
  if(s==INVALID_SOCKET){ log_line("socket failed"); return 1; }
#else
  int s = socket(AF_INET, SOCK_DGRAM, 0);
  if(s<0){ perror("socket"); return 1; }
#endif

  struct sockaddr_in srv; memset(&srv,0,sizeof srv);
  srv.sin_family=AF_INET; srv.sin_addr.s_addr=htonl(0); srv.sin_port=htons(port);
#ifdef _WIN32
  if(bind(s,(struct sockaddr*)&srv,sizeof srv)==SOCKET_ERROR){ log_line("bind failed"); return 1; }
#else
  if(bind(s,(struct sockaddr*)&srv,sizeof srv)<0){ perror("bind"); return 1; }
#endif

  log_line("Server listening UDP %d", port);

  for(;;){
    struct job *j=(struct job*)malloc(sizeof *j);
    j->cl=sizeof j->cli;
#ifdef _WIN32
    int n = recvfrom(s,(char*)j->buf,(int)sizeof j->buf,0,(struct sockaddr*)&j->cli,&j->cl);
    if(n==SOCKET_ERROR){ free(j); continue; }
#else
    ssize_t n = recvfrom(s,j->buf,sizeof j->buf,0,(struct sockaddr*)&j->cli,&j->cl);
    if(n<=0){ free(j); continue; }
#endif
    j->n=(size_t)n; j->s=s;

    pthread_t th; pthread_create(&th,NULL,worker,j); pthread_detach(th);
  }

  logger_close();
#ifdef _WIN32
  closesocket(s); WSACleanup();
#else
  close(s);
#endif
  return 0;
}
