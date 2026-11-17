#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#include<stdint.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<errno.h>
#include<fcntl.h>
#include<pthread.h>

#define PORT 7
#define ADDR "0.0.0.0"
#define MAX_EVENTS 4096
#define MAX_LISTEN 128


volatile int server_socket;
volatile int epoll_fd;

__attribute__((noreturn,cold)) void DieWithError(char *errorMessage);



int main(void){
    server_socket=socket(PF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(__builtin_expect(server_socket==-1,0))DieWithError("socket failed");
    int flags=fcntl(server_socket,F_GETFL);
    if(__builtin_expect(flags==-1,0))DieWithError("fcntl in F_GETFL failed");
    if(__builtin_expect(fcntl(server_socket,F_SETFL,flags|O_NONBLOCK|O_CLOEXEC)==-1,0))DieWithError("fcntl in F_SETFL failed");
    //非同期化完了
    struct sockaddr_in server_address;
    memset(&server_address,0,sizeof(struct sockaddr_in));
    server_address.sin_family=PF_INET;
    server_address.sin_port=htons(PORT);
    if(__builtin_expect(inet_pton(PF_INET,ADDR,&server_address.sin_addr)==-1,0))DieWithError("inet_pton failed");
    if(__builtin_expect(bind(server_socket,(struct sockaddr*)&server_address,sizeof(struct sockaddr_in))==-1,0))DieWithError("bind failed");
    //epoll setting phase
    epoll_fd=epoll_create1(EPOLL_CLOEXEC);
    if(__builtin_expect(epoll_fd==-1,0))DieWithError("epoll_create1 failed");
    struct epoll_event events,ev[MAX_EVENTS];
    events.events=EPOLLIN|EPOLLET;
    events.data.fd=server_socket;
    if(__builtin_expect(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,server_socket,&events)==-1,0))DieWithError("epoll_ctl failed from initialize phase");
    if(__builtin_expect(listen(server_socket,MAX_LISTEN)==-1,0))DieWithError("listen failed");
    printf("main loop start!\n");
    //main loop
    char buffer[1024];
    for(;;){
        int epoll_answer=epoll_wait(epoll_fd,ev,MAX_EVENTS,-1);
        if(__builtin_expect(epoll_answer==-1,0))DieWithError("epoll_wait failed");
        for(int i=0;i<epoll_answer;i++){
            if(ev[i].data.fd==server_socket){
                //new connection
                int clntsock=accept4(server_socket,NULL,NULL,SOCK_CLOEXEC|SOCK_NONBLOCK);
                if(__builtin_expect(clntsock==-1,0))DieWithError("accept4 failed");
                //epoll reg
                events.data.fd=clntsock;
                events.events=EPOLLIN|EPOLLET;
                if(__builtin_expect(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,clntsock,&events)==-1,0))DieWithError("epoll_ctl failed");
                continue;
            }else{
                ssize_t ans_size=recv(ev[i].data.fd,&buffer,1023,MSG_NOSIGNAL);
                if(__builtin_expect(ans_size==-1,0)){
                    fprintf(stderr,"recv failed: %s\n",strerror(errno));
                    epoll_ctl(epoll_fd,EPOLL_CTL_DEL,ev[i].data.fd,NULL);
                    shutdown(ev[i].data.fd,SHUT_RDWR);
                    close(ev[i].data.fd);
                    continue;
                }else{
                    if(__builtin_expect(__builtin_memcmp(buffer,"exit",4)==0,0)){
                        fprintf(stdout,"client scoket close");
                        epoll_ctl(epoll_fd,EPOLL_CTL_DEL,ev[i].data.fd,NULL);
                        shutdown(ev[i].data.fd,SHUT_RDWR);
                        close(ev[i].data.fd);
                        continue;
                    }else{
                        ssize_t ans=send(ev[i].data.fd,buffer,ans_size,MSG_NOSIGNAL);
                        if(__builtin_expect(ans==-1,0)){
                            fprintf(stderr,"send failed: %s",strerror(errno));
                            epoll_ctl(epoll_fd,EPOLL_CTL_DEL,ev[i].data.fd,NULL);
                            shutdown(ev[i].data.fd,SHUT_RDWR);
                            close(ev[i].data.fd);
                            continue;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

__attribute__((noreturn,cold)) void DieWithError(char *errorMessage){
    fprintf(stderr,"%s: %s",errorMessage,strerror(errno));
    exit(EXIT_FAILURE);
    __builtin_unreachable();
}