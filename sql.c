#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>	
#include "sql.h"

#define DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

int enable_wal_mode(sqlite3 *db) {
    char *errmsg = NULL;
    if (sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, &errmsg) != SQLITE_OK) {
        ERROR("Failed to enable WAL mode: %s", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }
    DEBUG("WAL mode enabled successfully!");
    return 0;
}



void insertRecord(sqlite3 *db, const char *table_name, SensorData *sensor) {
    char SQLInsertData[128];
    char *errmsg;
    snprintf(SQLInsertData, sizeof(SQLInsertData),
             "Insert into %s(humidity, temperature, CO2, light) values(%d, %f, %d, %d)",
             table_name, sensor->humidity, sensor->temperature, sensor->CO2, sensor->light);
    if (sqlite3_exec(db, SQLInsertData, NULL, NULL, &errmsg) != SQLITE_OK) {
        ERROR("insert record: %s", errmsg);
    } else {
        DEBUG("insert record %d success!\n", sensor->id);
    }
}

void do_insert(sqlite3 *db, const char *table_name, SensorData *sensors, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        insertRecord(db, table_name, &sensors[i]);
    }
}


//创建数据库并打开
sqlite3 *initDataBase(const char *db_name) {
	sqlite3 *db = NULL;

	if (sqlite3_open(db_name, &db) != SQLITE_OK) {//没有的话就创建
        ERROR("%s\n", sqlite3_errmsg(db));
		return NULL;
	}
	DEBUG("OPEN DataBase success!\n");
	return db;
}

//创建表格
void createTable(sqlite3 *db, const char *table_name) {
	char *errmsg;//存储错误信息
	char SQLCreateTable[128];//存储创建表格的SQL语句
	snprintf(SQLCreateTable, sizeof(SQLCreateTable), "create table if not exists %s("
			 "id INTEGER PRIMARY KEY AUTOINCREMENT, "
			 "humidity int, temperature real, CO2 int, light int);", table_name);


	if (sqlite3_exec(db, SQLCreateTable, \
	NULL, NULL, &errmsg) != SQLITE_OK) {
		ERROR("%s\n", errmsg);
	} else {
		DEBUG("create or open table success!\n");
	}
}

//关闭数据库
void releaseDataBase(sqlite3 *db) {
	sqlite3_close(db);
}


// 查询最近 N 条历史记录 (按 ID 倒序，最新数据排在最前面)
int get_history_records(sqlite3 *db, const char *table_name, SensorData *out_sensors, int max_count, int *out_count) {
   //outcount 是实际取到的数据行数
  // 1. 参数合法性校验：防止空指针访问或无效的查询条数
  if (db == NULL || table_name == NULL || out_sensors == NULL || out_count == NULL || max_count <= 0) {
    return -1;
  }

  // 2. 拼接 SQL 语句：查询指定表，按 id 从大到小 (DESC) 倒序排列，最多限制查 max_count 条
  char sql[256];
  snprintf(sql, sizeof(sql),
           "SELECT id, humidity, temperature, CO2, light FROM %s ORDER BY id DESC LIMIT %d;",
           table_name, max_count);

  // 3. 编译 SQL 语句 (sqlite3_prepare_v2)
  // stmt (statement) 是 SQLite 的“字节码句柄”，类似编译出来的二进制执行指令
  sqlite3_stmt *stmt;
  //编译 SQL 菜谱”。把 SQL 字符串（如 SELECT...）编译成数据库内部能执行的字节码，返回一个句柄 stmt。可以理解为：把菜名变成能做的步骤单。
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    ERROR("Failed to prepare statement: %s", sqlite3_errmsg(db));
    return -1;
  }

  // 4. 循环单步执行 (sqlite3_step)，每次获取数据库中的一行记录
  int count = 0;
  // sqlite3_step 返回 SQLITE_ROW 表示成功拿到了新的一行数据
  
  // “执行一步/取一行”。拿编译好的 stmt 去数据库里运行，每次调用取出一行结果。类似迭代器的 next()
  while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count) {
    // 按 SELECT 后的列顺序提取字段 (索引从 0 开始):
    // 0: id (int)
    // 1: humidity (int)
    // 2: temperature (double/real)
    // 3: CO2 (int)
    // 4: light (int)
    out_sensors[count].id = sqlite3_column_int(stmt, 0);
    out_sensors[count].humidity = sqlite3_column_int(stmt, 1);
    out_sensors[count].temperature = sqlite3_column_double(stmt, 2);
    out_sensors[count].CO2 = sqlite3_column_int(stmt, 3);
    out_sensors[count].light = sqlite3_column_int(stmt, 4);
    
    count++; // 计数器加 1，指向下一个数组位置
  }

  // 5. 释放句柄资源：销毁 stmt 编译句柄，防止内存泄露，销毁编译句柄”。释放 stmt 占用的内存。必须调用，否则内存泄漏。
  sqlite3_finalize(stmt);

  // 6. 将实际查询到的记录数量通过指针传回给调用者
  *out_count = count;
  DEBUG("Successfully queried %d history records", count);
  return 0; // 成功返回 0
}

// 查询最新 1 条记录 (复用上面的历史查询函数，限制条数为 1)
int get_latest_record(sqlite3 *db, const char *table_name, SensorData *sensor) {
  int count = 0;
  // 传入 max_count = 1，如果成功查出 1 条记录，说明获取成功
  if (get_history_records(db, table_name, sensor, 1, &count) == 0 && count == 1) {
    return 0;
  }
  return -1; // 数据库为空或查询失败返回 -1
}
