#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <time.h>
#include <sys/mman.h>
#include <pthread.h>
#include "queue.h"
#include <signal.h>
#include <stdio.h>


void init_queue(queue_t* queue) {
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
}

// 添加数据到队列
void enqueue_data(queue_t* queue, uint16_t* data, size_t length) {
    queue_node_t* new_node = (queue_node_t*)malloc(sizeof(queue_node_t));
    new_node->data = (char*)malloc(length);
    memcpy(new_node->data, data, length);
    new_node->length = length;
    new_node->next = NULL;

    if (queue->rear == NULL) {
        queue->front = queue->rear = new_node;
    } else {
        queue->rear->next = new_node;
        queue->rear = new_node;
    }

    queue->size++;
}

// 取出队列中的数据（不删除）
queue_node_t* peek_queue(queue_t* queue) {
    if (queue->front == NULL) {
        return NULL;  // 队列为空
    }
    return queue->front;
}

// 删除队列头部数据
void dequeue_data(queue_t* queue) {
    if (queue->front == NULL) {
        return;  // 队列为空
    }
    queue_node_t* temp = queue->front;
    queue->front = queue->front->next;
    if (queue->front == NULL) {
        queue->rear = NULL;
    }
    free(temp->data);
    free(temp);

    queue->size--;
}

// 销毁队列
void destroy_queue(queue_t* queue) {
    while (queue->front != NULL) {
        dequeue_data(queue);
    }
}

// 设置 socket 为非阻塞
void set_socket_nonblocking(int sock_fd) {
    int flags = fcntl(sock_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL) failed");
        return;
    }

    if (fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl(F_SETFL) failed");
    }
}

// 非阻塞发送队列中的数据
void process_send_queue(int sock_fd, queue_t* queue) {
    //printf("Queue size first: %d\n", queue->size);
        static clock_t last_print_time = 0; // 记录上一次打印的时间

    while (queue->front != NULL) {
        queue_node_t* node = peek_queue(queue);
        ssize_t bytes_sent = send(sock_fd, node->data, node->length, 0);

        if (bytes_sent == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // socket 暂时不可写，等待下一次机会
                return;
            } else {
                // 发送时出错，处理错误
                perror("send failed");
                return;
            }
        }

        if (bytes_sent < node->length) {
            // 没有完全发送，更新剩余的未发送数据
            memmove(node->data, node->data + bytes_sent, node->length - bytes_sent);
            node->length -= bytes_sent;
            return;
        }

        // 完全发送，移除队列头部数据
        dequeue_data(queue);

        // 获取当前时间并计算与上一次打印的时间差
        clock_t current_time = clock();
        double time_elapsed = (double)(current_time - last_print_time) / CLOCKS_PER_SEC;

        // 如果已超过 0.5 秒，打印队列中的数据
        if (time_elapsed >= 1) {
            printf("Queue size: %d\n", queue->size); // 假设 queue->size 表示队列中的数据个数
            last_print_time = current_time;
        }
    }
}

// 使用 select 监控并发送数据
void monitor_and_send(int sock_fd, queue_t* queue) {
    // 检查队列是否有数据
    if (queue->front == NULL) {
        return;  // 队列为空，直接返回，不进行发送
    }

    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(sock_fd, &writefds);

    struct timeval timeout;
    timeout.tv_sec = 1;  // 1秒超时
    timeout.tv_usec = 0;

    int ready = select(sock_fd + 1, NULL, &writefds, NULL, &timeout);
    if (ready > 0 && FD_ISSET(sock_fd, &writefds)) {
        // socket 可写，处理发送队列
        process_send_queue(sock_fd, queue);
    }
}


int read_memory_data(uint32_t readAddr, uint16_t* buf, uint32_t len) {
    int i, fd;
    off_t addr = readAddr;
    void *map_base = NULL;  // 显式初始化 map_base
    long unsigned int offset_len = 0;
    uint16_t* virt_addr;

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
        FATAL;  // 错误处理
    }

    /* Map one page */ // 将内核空间映射到用户空间
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
    if(map_base == (void *) -1)
    {
        printf("Error at line %d, file %s\n", __LINE__, __FILE__);
        close(fd);
        return 0;
    }

    for (i = 0; i < len; i++)
    {
        // 翻页处理
        if(offset_len >= MAP_MASK)
        {
            offset_len = 0;
            if(munmap(map_base, MAP_SIZE) == -1)
            {
                printf("Error at line %d, file %s\n", __LINE__, __FILE__);
                close(fd);
                return 0;
            }
            map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);
            if(map_base == (void *) -1)
            {
                printf("Error at line %d, file %s\n", __LINE__, __FILE__);
                close(fd);
                return 0;
            }
        }

        virt_addr = map_base + (addr & MAP_MASK);	// 将内核空间映射到用户空间操作
        buf[i] = *((uint16_t *) virt_addr);	// 读取16位数据
        addr += 2;  // 每次移动2字节（16位）
        offset_len += 2;
    }

    if(munmap(map_base, MAP_SIZE) == -1)
    {
        printf("Error at line %d, file %s\n", __LINE__, __FILE__);
        close(fd);
        return 0;
    }
    close(fd);
    return i;
}

int send_string_data_to_queue(uint32_t start_address, uint32_t data_numbers, queue_t *queue, int repetitions) {

    uint16_t buffer[1024]; // 数据数量即为要读取的16位数据的数量
    int total_sent = 0;
    //uint32_t length = data_numbers;
    uint32_t current_address = start_address;

    //start = clock();
    for (int r = 0; r < repetitions; r++) {
        uint32_t len = data_numbers;
        current_address = start_address;
        while(len > 0){
            int words_to_read = len < 512 ? len : 512;  // 每次读取最多500个16位数据
            // 从内存中读取数据
            int words_read = read_memory_data(current_address, buffer, words_to_read);
            if (words_read < 0) {
                return -1;
            }
            // 将数据转换为网络字节序（大端格式）
            for (int i = 0; i < words_read; i++) {
                //printf("buffer[%d] is 0x%04x\n", i,buffer[i]);
                buffer[i] = (buffer[i] << 8) | (buffer[i] >> 8);
                
            }
            // 将数据发送到队列中
            enqueue_data(queue, buffer, words_read*2);
            usleep(1000);
            // 更新地址和剩余长度
            current_address += words_read*2;  // 每次递增16位数据的长度（2字节）
            //printf("current_address is 0x%08x\n", current_address);
            len -= words_read;  // 更新剩余长度    
        }
        //printf("repetitions is %d\n", r);
    }    

    //nd = clock();
    //cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    //printf("CPU time used: %f seconds\n", cpu_time_used);

    //uint16_t flag = 0xFF;
    //enqueue_data(queue, &flag, sizeof(flag));


    return total_sent;
}



void *tcp_init(void *arg) {

    // 创建 socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket creation failed");
        pthread_exit(NULL);  // 用 pthread_exit 退出线程
    }

    // 设置服务器地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2020);
    server_addr.sin_addr.s_addr = inet_addr("192.168.1.10");

    // 连接服务器
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        close(sock_fd);
        pthread_exit(NULL);  // 用 pthread_exit 退出线程
    }

    // 设置 socket 为非阻塞
    set_socket_nonblocking(sock_fd);

    // 初始化发送队列
    init_queue(&send_queue);
    //char message[100];

    printf("tcp_init sucess ");


    while(1){     
        monitor_and_send(sock_fd, &send_queue);
        printf("queue is :%d",send_queue.size);
        sleep(1);  // 延迟100ms，避免高 CPU 占用

    }
    // 关闭 socket
    //close(sock_fd);
    // 销毁队列
    //destroy_queue(&send_queue);


    return 0;
}

void handle_sigint(int sig) {
    printf("\n捕获到 SIGINT 信号 Ctrl+C 执行清理操作...\n");
    // 执行清理操作，例如关闭资源、保存数据等
    //free_memory();
    close_tcp_client();
    exit(0);  // 正常退出
}

void close_tcp_client(){
    close(sock_fd);
    printf("Tcp_client_closed!\n");
    // 销毁队列
    destroy_queue(&send_queue);
    printf("Queue_destroyed!\n");
    //return 0;
}

