#include "store.h"
#include <string.h>
#include <pthread.h>
#include <stdio.h>

#define MAX_ITEMS 16
#define KEY_MAX   64
#define VAL_MAX   1024

typedef struct { char key[KEY_MAX]; char val[VAL_MAX]; int used; } kv_t;
static kv_t tab[MAX_ITEMS];
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

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

int store_upsert(const char *key, const char *json){
  pthread_mutex_lock(&mtx);
  int i = find_slot(key);
  if(i<0){ i = find_free(); if(i<0){ pthread_mutex_unlock(&mtx); return -1; } }
  snprintf(tab[i].key, KEY_MAX, "%s", key);
  snprintf(tab[i].val, VAL_MAX, "%s", json);
  tab[i].used = 1;
  pthread_mutex_unlock(&mtx);
  return 0;
}

int store_get(const char *key, char *out, int outsz){
  int rc=-1;
  pthread_mutex_lock(&mtx);
  int i = find_slot(key);
  if(i>=0){
    snprintf(out, outsz, "%s", tab[i].val);
    rc=0;
  }
  pthread_mutex_unlock(&mtx);
  return rc;
}

int store_delete(const char *key){
  int rc=-1;
  pthread_mutex_lock(&mtx);
  int i = find_slot(key);
  if(i>=0){ tab[i].used=0; tab[i].key[0]=0; tab[i].val[0]=0; rc=0; }
  pthread_mutex_unlock(&mtx);
  return rc;
}
