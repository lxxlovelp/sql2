#ifndef SQL_CGI_H
#define SQL_CGI_H

#include "/home/xingxinliao/lab/tool/include/sqlite3.h"
#include "sql.h"

typedef struct {
    sqlite3 *db;
    const char *table_name;
} ThreadArgs;

int send_data_memery(sqlite3 *db, const char *table_name);
void *shm_cgi_thread(void *arg);

#endif