#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>

int main(){

    signal(SIGINT, handle_sigint);

    pthread_t thread_id;
    int ret;

    // 创建线程
    ret = pthread_create(&thread_id, NULL, tcp_init, NULL);
    if (ret) {
        fprintf(stderr, "Error creating thread: %d\n", ret);
        return EXIT_FAILURE;
    }

    // 设置线程的 CPU 亲和性，绑定到 CPU 0
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);  // 将线程绑定到 CPU 0
    ret = pthread_setaffinity_np(thread_id, sizeof(cpu_set_t), &cpuset);
    if (ret) {
        fprintf(stderr, "Error setting thread affinity: %d\n", ret);
    }

    uint32_t memory_address;
    uint32_t data_numbers;
    int repetitions;
    char conmand[10];
    char token[BUFFER_SIZE];  // 用于保存用户输入

    while (1) {
        printf("Please use: send <address> <data_numbers> <repetitions>\n");
        if (scanf("%9s %x %d %d", conmand, &memory_address, &data_numbers, &repetitions) == 4) {
            if(strcmp(conmand, "send") == 0){
                init_queue(&send_queue);
                if (send_string_data_to_queue(memory_address, data_numbers, &send_queue, repetitions) < 0) {
                    printf("Failed to send memory data.\n");
                }
                printf("send data successful!\n");
            }
        } else if(send(sock_fd, token, strlen(token) - 1, 0) < 0) {
            perror("send");
                //goto err;
        } 
    }
    // 等待线程结束
    pthread_join(thread_id, NULL);
    return EXIT_SUCCESS;
     
}