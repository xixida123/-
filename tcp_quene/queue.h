
#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>
#include <time.h>
#include <arpa/inet.h>

// 常量、宏定义和类型定义
#define FATAL do { fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
  __LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)

#define MAP_SIZE 4096UL  // 映射的内存区大小（4KB）。
#define MAP_MASK (MAP_SIZE - 1)  // 掩码，用于对齐地址到页面边界。
#define BUFFER_SIZE 1024

// 定义队列节点
typedef struct queue_node {
    char* data;            // 存储数据的指针
    size_t length;         // 数据长度
    struct queue_node* next;
} queue_node_t;

// 定义队列
typedef struct {
    queue_node_t* front;   // 队列头部
    queue_node_t* rear;    // 队列尾部
    int size;
} queue_t;

int sock_fd;
struct sockaddr_in server_addr;
queue_t send_queue;
clock_t start, end;
double cpu_time_used;



void init_queue(queue_t* queue);
void enqueue_data(queue_t* queue, uint16_t* data, size_t length);
queue_node_t* peek_queue(queue_t* queue);
void dequeue_data(queue_t* queue);
void destroy_queue(queue_t* queue);
void set_socket_nonblocking(int sock_fd);
void process_send_queue(int sock_fd, queue_t* queue);
void monitor_and_send(int sock_fd, queue_t* queue);
void close_tcp_client();
void *tcp_init(void *arg);
void handle_sigint(int sig);
int read_memory_data(uint32_t readAddr, uint16_t* buf, uint32_t len);
int send_string_data_to_queue(uint32_t start_address, uint32_t data_numbers, queue_t *queue, int repetitions);

#endif // !QUEUE_H
