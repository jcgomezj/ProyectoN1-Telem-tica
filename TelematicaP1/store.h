#ifndef STORE_H
#define STORE_H

/* almacén simple: guarda valor JSON por recurso (path) */
int store_upsert(const char *key, const char *json);
int store_get(const char *key, char *out, int outsz);
int store_delete(const char *key);

#endif
