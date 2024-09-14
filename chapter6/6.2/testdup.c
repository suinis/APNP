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

int main(int argc, char* argv[]){
    if(argc != 3){
        printf("usage : %s ip_address port_number \n", basename(argv[0]));
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
        close(STDOUT_FILENO); //关闭标准输出: fd = 1
        system("lsof -p $$"); //查看该进程的FD等信息，未执行dup之前，有一行显示：
        //bash    27671 root    1u   CHR  136,3      0t0      6 /dev/pts/3
        dup(connfd); //创建新的文件描述符：fd = 1，作用同connfd，用于同客户端通信的描述符，也即目前fd = 1不在指标准输出，而是指同客户端的连接
        printf("abcd\n"); 
        system("lsof -p $$"); //执行dup之后，对应一行更新为：
        //sh      27671 root    1u  IPv4 607950      0t0    TCP ubuntu:telnet->192.168.80.129:51386 (ESTABLISHED)
        close(connfd);
    }

    close(sockfd);
    return 0;
}