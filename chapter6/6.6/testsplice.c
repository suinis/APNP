#define _GNU_SOURCE         /* use <bits/fcntl-linux.h> */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <fcntl.h>

int main(int argc, char* argv[]){
    if(argc != 3){
        printf("usage : %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &servaddr.sin_addr);

    int ret = bind(sockfd, (struct sockaddr*)& servaddr, sizeof(servaddr));
    assert(ret != -1);

    ret = listen(sockfd, 5);
    assert(ret != -1);

    struct sockaddr_in clientaddr;
    socklen_t clientaddr_sz = sizeof(clientaddr);
    int connfd = accept(sockfd, (struct sockaddr*)& clientaddr, &clientaddr_sz);
    if(connfd < 0){
        printf("errno is: %d\n", errno);
    }
    else{
        int pipefd[2];
        int ret = pipe(pipefd);
        assert(ret != -1);
        sleep(5); //用于telnet客户端启动时输入数据的时间
        ret = splice(connfd, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
        assert(ret != -1);
        ret = splice(pipefd[0], NULL, connfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
        assert(ret != -1);
        close(connfd);
    }

    close(sockfd);
    return 0;
}