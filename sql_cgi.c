#include "sql_cgi.h"
#include "sql.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SHM_SIZE 1024
#define FIFO_PATH "/tmp/request_fifo"

// V操作：唤醒
void V(int semid, int num) {
  struct sembuf op = {num, 1, 0};
  semop(semid, &op, 1);
}
//将数据写入共享内存
int send_data_memery(sqlite3 *db, const char *table_name) {
  printf("B: 后台服务启动，正在监听 FIFO 管道等待 CGI 请求 (防粘连模式)...\n");
  mkfifo(FIFO_PATH, 0666); // 确保管道存在

  int fifo_fd = open(FIFO_PATH, O_RDWR);
  if (fifo_fd < 0) {
    perror("B: 打开管道失败");
    return -1;
  }

  while (1) {
    key_t a_key;

    int bytes = read(fifo_fd, &a_key, sizeof(key_t));

    if (bytes != sizeof(key_t)) {
      continue; // 如果读出来的字节数不对，直接忽略
    }

    printf("B: 收到一个 CGI 的请求，对方的专属 KEY 是: %d\n", a_key);

    int shmid = shmget(a_key, SHM_SIZE, 0666);
    if (shmid < 0) {
      perror("B: 找不到对方的共享内存");
      continue;
    }
    char *addr = shmat(shmid, NULL, 0);
    if (addr == (char *)-1) {
      perror("B: shmat 挂载共享内存失败");
      continue;
    }

    int semid = semget(a_key, 1, 0666);

    // 4. 往对方的共享内存里传东西 (去查数据库，把结果写进去)
    SensorData sensordata;
    memset(&sensordata, 0, sizeof(SensorData));
    if (get_latest_record(db, table_name, &sensordata) == 0) {
      memcpy(addr, &sensordata, sizeof(SensorData));
      printf("B: 最新数据 (ID:%d, Temp:%.2f, Humidity:%d, CO2:%d, Light:%d) "
             "已写入 KEY=%d 的共享内存！\n",
             sensordata.id, sensordata.temperature, sensordata.humidity,
             sensordata.CO2, sensordata.light, a_key);
    } else {
      printf("B: 数据库尚无数据或查询失败！\n");
    }

    // 5. 传完了，B 断开对这个共享内存的挂载
    shmdt(addr);

    // 6. 用 V 操作唤醒对应的 A 进程，让他自己去读数据然后删除共享内存
    V(semid, 0);
    printf("B: 已唤醒 A，继续监听管道...\n\n");
  }

  close(fifo_fd);
  return 0;
}

void *shm_cgi_thread(void *arg) {
  ThreadArgs *args = (ThreadArgs *)arg;
  send_data_memery(args->db, args->table_name);
  return NULL;
}