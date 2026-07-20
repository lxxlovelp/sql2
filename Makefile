# SQLite3 自定义路径配置（如无需自定义路径，保留空值或保持默认即可）
# 例如: SQLITE_DIR := /home/xingxinliao/lab/tool
SQLITE_DIR := /home/xingxinliao/lab/tool

# 自动检测自定义路径是否存在，存在时自动拼接 -I 和 -L 编译选项
ifneq ($(wildcard $(SQLITE_DIR)/include),)
    SQLITE_CFLAGS  := -I$(SQLITE_DIR)/include
    SQLITE_LDFLAGS := -L$(SQLITE_DIR)/lib
else
    SQLITE_CFLAGS  :=
    SQLITE_LDFLAGS :=
endif

# 编译器及选项设置
CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -g $(SQLITE_CFLAGS)
LDFLAGS := $(SQLITE_LDFLAGS) -lsqlite3 -lpthread

# 目标文件与源文件定义
TARGET  := receive_sql
SRCS    := main.c sql.c sql_cgi.c 
OBJS    := $(SRCS:.c=.o)
HEADERS := sql.h sql_cgi.h

# 默认规则：构建可执行程序
all: $(TARGET)

# 链接规则
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# 编译规则
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# 清理目标
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
