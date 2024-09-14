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
    if(argc <= 2){
        printf("usage : %s ip_addresss port_number\n", basename(argv[0]));
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
        ret = recv(connfd, buf, BUF_SIZE - 1, 0);
        printf("got %d bytes of normal data '%s'\n", ret, buf);

        memset(buf, '\0', BUF_SIZE);
        ret = recv(connfd, buf, BUF_SIZE - 1, MSG_OOB);
        printf("got %d bytes of oob data '%s'\n", ret, buf);

        memset(buf, '\0', BUF_SIZE);
        ret = recv(connfd, buf, BUF_SIZE - 1, 0);
        printf("got %d bytes of normal data '%s'\n", ret, buf);

        close(connfd);
    }

    close(sockfd);
    return 0;
}