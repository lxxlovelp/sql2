#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "sql.h"
#include "sql_cgi.h"

#define SHM_SIZE 1024
#define FIFO_PATH "/tmp/request_fifo"

// V操作：唤醒
void V(int semid, int num) {
  struct sembuf op = {num, 1, 0};
  semop(semid, &op, 1);
}

int send_data_memery(void) {
  printf("B: 后台服务启动，正在监听 FIFO 管道等待 CGI 请求 (防粘连模式)...\n");
  mkfifo(FIFO_PATH, 0666); // 确保管道存在

  // 【核心技巧】：使用 O_RDWR 打开，而不是 O_RDONLY！
  // 如果用 O_RDONLY，当所有进程 A 都写完退出后，B 的 read 会返回 0 (EOF) 导致死循环狂转。
  // 用 O_RDWR 相当于 B 自己也保留了一个写句柄，管道永远不会 EOF，read 会完美地安静阻塞。
  // 另外，我们把 open 移出 while 循环，保证一次性可以读取任意多个挤压的请求，不会中途丢弃。
  int fifo_fd = open(FIFO_PATH, O_RDWR);
  if (fifo_fd < 0) {
    perror("B: 打开管道失败");
    exit(1);
  }

  while (1) {
    key_t a_key;

    // 【核心】解决粘连：严格按照 sizeof(key_t) 定长读取！
    // 即使管道里挤压了 10 个 A 进程发来的 40 字节，这里也只会精准地切下前面的 4 个字节
    int bytes = read(fifo_fd, &a_key, sizeof(key_t));

    if (bytes != sizeof(key_t)) {
      continue; // 如果读出来的字节数不对，直接忽略
    }

    printf("B: 收到一个 CGI 的请求，对方的专属 KEY 是: %d\n", a_key);

    // 2. 根据 A 传过来的 key，挂载 A 创建的专属共享内存
    int shmid = shmget(a_key, SHM_SIZE, 0666);
    if (shmid < 0) {
      perror("B: 找不到对方的共享内存");
      continue;
    }
    char *addr = shmat(shmid, NULL, 0);

    // 3. 根据 A 传过来的 key，获取 A 的专属信号量
    int semid = semget(a_key, 1, 0666);

    // 4. 往对方的共享内存里传东西 (去查数据库，把结果写进去)
    sprintf(addr, sensordata, a_key);
    printf("B: 数据已写入 KEY=%d 的共享内存！\n", a_key);

    // 5. 传完了，B 断开对这个共享内存的挂载
    shmdt(addr);

    // 6. 用 V 操作唤醒对应的 A 进程，让他自己去读数据然后删除共享内存
    V(semid, 0);
    printf("B: 已唤醒 A，继续监听管道...\n\n");
  }

  close(fifo_fd);
  return 0;
}