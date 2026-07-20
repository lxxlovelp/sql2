#include <stdio.h>
#include <stdlib.h>
#include "/home/xingxinliao/lab/tool/include/sqlite3.h"
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "sql.h"
#include "sql_cgi.h"

#define DB_PATH     "/home/xingxinliao/Documents/SQL/sensor_data.db"
#define TABLE_NAME  "sensor"

SensorData sensordata;

int main(void) {
    sqlite3 *db = initDataBase(DB_PATH);
    if (db == NULL) return EXIT_FAILURE;

    /* 创建/打开表 */
    createTable(db, TABLE_NAME);

    /* 获取消息队列 */
    key_t key = ftok("/tmp", 556);
    if (key == -1) {
        perror("ftok");
        releaseDataBase(db);
        return EXIT_FAILURE;
    }
    
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("msgget");
        releaseDataBase(db);
        return EXIT_FAILURE;
    }

   
    while (1) {
        struct msgbuf msg_recv_struct;
        ssize_t r = msgrcv(msgid, &msg_recv_struct, sizeof(msg_recv_struct.text), 1, 0);
        if (r == -1) {
            perror("msgrcv");
            break;
        }
        //从队列里面拿数据后插入数据库
        insertRecord(db, TABLE_NAME, &msg_recv_struct.text);
        //取最新的一条数据发到共享内存
        get_latest_record(db, TABLE_NAME,&sensordata);
        send_data_memery();
    }

    releaseDataBase(db);
    return EXIT_SUCCESS;
}