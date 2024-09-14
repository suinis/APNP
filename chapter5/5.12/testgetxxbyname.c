#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

int main(int argc, char* argv[]){
    assert(argc == 2);
    char* host = argv[1];
    // 获取目标主机地址信息
    struct hostent* hostinfo = gethostbyname(host);
    assert(hostinfo);
    // 获取daytime服务信息
    struct servent* servinfo = getservbyname("daytime", "tcp");
    assert(servinfo);
    printf("daytime port is %d\n", ntohs(servinfo->s_port));

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = servinfo->s_port;
    servaddr.sin_addr = *(struct in_addr*)*hostinfo->h_addr_list;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int ret = connect(sockfd, (struct sockaddr*)& servaddr, sizeof(servaddr));
    assert(ret != -1);

    char buf[128];
    ret = recv(sockfd, buf, sizeof(buf), 0);
    assert(ret > 0);
    buf[ret] = '\0';
    printf("the day time is : %s\n", buf);
    close(sockfd);

    return 0;
}