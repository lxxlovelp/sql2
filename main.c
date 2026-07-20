#include <stdio.h>
#include <stdlib.h>
#include "/home/xingxinliao/lab/tool/include/sqlite3.h"
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include "sql.h"
#include "sql_cgi.h"

#define DB_PATH     "sensor_data.db"
#define TABLE_NAME  "sensor"

typedef struct {
    int msgid;
    const char *db_path;
    const char *table_name;
} QueueThreadArgs;

// 线程 1：从消息队列接收数据并入库
void *queue_consumer_thread(void *arg) {
    QueueThreadArgs *args = (QueueThreadArgs *)arg;
    sqlite3 *db = initDataBase(args->db_path);
    if (!db) {
        fprintf(stderr, "Queue thread: Failed to open DB\n");
        return NULL;
    }

    printf("[Queue Thread] 消息队列消费线程已启动...\n");
    while (1) {
        struct sensor_msgbuf msg_recv_struct;
        ssize_t r = msgrcv(args->msgid, &msg_recv_struct, sizeof(msg_recv_struct.text), 1, 0);
        if (r == -1) {
            perror("msgrcv");
            break;
        }
        // 从队列获取数据后插入数据库
        insertRecord(db, args->table_name, &msg_recv_struct.text);
    }

    releaseDataBase(db);
    return NULL;
}

// 线程 2：监听 FIFO 并响应 CGI 请求写共享内存
void *shm_server_thread(void *arg) {
    const char *db_path = (const char *)arg;
    sqlite3 *db = initDataBase(db_path);
    if (!db) {
        fprintf(stderr, "CGI thread: Failed to open DB\n");
        return NULL;
    }

    printf("[CGI Thread] 共享内存 CGI 响应线程已启动...\n");
    send_data_memery(db, TABLE_NAME);

    releaseDataBase(db);
    return NULL;
}

int main(void) {
    // 1. 初始化数据库及建表
    sqlite3 *db_init = initDataBase(DB_PATH);
    if (db_init == NULL) return EXIT_FAILURE;

    enable_wal_mode(db_init);
    createTable(db_init, TABLE_NAME);
    releaseDataBase(db_init);

    // 2. 获取消息队列
    key_t key = ftok("/tmp", 556);
    if (key == -1) {
        perror("ftok");
        return EXIT_FAILURE;
    }

    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("msgget");
        return EXIT_FAILURE;
    }

    // 3. 创建并发子线程
    pthread_t tid_queue, tid_shm;
    QueueThreadArgs qargs = {
        .msgid = msgid,
        .db_path = DB_PATH,
        .table_name = TABLE_NAME
    };

    if (pthread_create(&tid_queue, NULL, queue_consumer_thread, &qargs) != 0) {
        perror("pthread_create queue_consumer");
        return EXIT_FAILURE;
    }

    if (pthread_create(&tid_shm, NULL, shm_server_thread, (void *)DB_PATH) != 0) {
        perror("pthread_create shm_server");
        return EXIT_FAILURE;
    }

    // 4. 等待子线程运行
    pthread_join(tid_queue, NULL);
    pthread_join(tid_shm, NULL);

    return EXIT_SUCCESS;
}