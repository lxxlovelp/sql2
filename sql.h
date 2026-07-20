#ifndef SQL_H
#define SQL_H

#include <sqlite3.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>	
#include <stddef.h>


typedef struct {
    int id;
    int humidity;
    double temperature;
    int CO2;
    int light;
} SensorData;

struct sensor_msgbuf {
    long type;
    SensorData text; // 只发送/接收一个 SensorData
};

int msg_recv_sensors(key_t key, long type, SensorData *sensors, size_t max_count, size_t *received_count);
int msg_send_sensors(key_t key, long type, SensorData *sensors, size_t count);
void do_insert(sqlite3 *db, const char *table_name, SensorData *sensors, size_t count);
sqlite3 *initDataBase(const char *db_name);
int enable_wal_mode(sqlite3 *db);
void createTable(sqlite3 *db, const char *table_name);
void releaseDataBase(sqlite3 *db);
void insertRecord(sqlite3 *db, const char *table_name, SensorData *sensor);
int get_history_records(sqlite3 *db, const char *table_name, SensorData *out_sensors, int max_count, int *out_count);
int get_latest_record(sqlite3 *db, const char *table_name, SensorData *sensor);

#endif