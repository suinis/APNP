#include <sys/socket.h>
#include <netinet/in.h>
#include  <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>

#define BUF_SIZE 2000

int main(int argc, char* argv[]){
    if(argc <= 3){
        printf("usage : %s ip_addresss port_number send_buffer_size\n", basename(argv[0]));
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

    int send_buffer_size = atoi(argv[3]);
    socklen_t len = sizeof(send_buffer_size);
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, sizeof(send_buffer_size));
    getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, &len);
    printf("the tcp send bufffer size after setting is %d\n", send_buffer_size);

    int ret = connect(sockfd, (struct sockaddr*)& serv_addr, sizeof(serv_addr));
    if(ret < 0){
        printf("connection failed!\n");
    }
    else{
        char buf[BUF_SIZE];
        memset(buf, 'a', BUF_SIZE);
        send(sockfd, buf, BUF_SIZE - 1, 0);
    }   

    close(sockfd);

    return 0;
}