#include <sys/socket.h>
#include <netinet/in.h>
#include  <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <errno.h>

#define BUF_SIZE 1024

int main(int argc, char* argv[]){
    if(argc <= 3){
        printf("usage : %s ip_addresss port_number recv_buffer_size\n", basename(argv[0]));
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(port);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    int recv_buffer_size = atoi(argv[3]);
    socklen_t len = sizeof(recv_buffer_size);
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, sizeof(recv_buffer_size));
    getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, &len);
    printf("the tcp receive buffer size after setting is %d\n", recv_buffer_size);

    int ret = bind(sockfd, (struct sockaddr*)& serv_addr, sizeof(serv_addr));
    assert(ret != -1);

    ret = listen(sockfd, 5);
    assert(ret != -1);

    struct sockaddr_in client;
    socklen_t client_addr_len = sizeof(client);
    int connfd = accept(sockfd, (struct sockaddr*)& client, &client_addr_len);
    if(connfd < 0){
        printf("errno is : %d\n", errno);
    }
    else{
        char buf[BUF_SIZE];
        memset(buf, '\0', BUF_SIZE);
        while (recv(connfd, buf, BUF_SIZE - 1, 0) > 0)
        {
            /* code */
        }
        
        close(connfd);
    }

    close(sockfd);
    return 0;
}