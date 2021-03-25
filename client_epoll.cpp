#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/epoll.h>

#define BUFFER_SIZE 64
#define MAX_EVENT_NUMBER 1024

char read_buf[BUFFER_SIZE];
bool m_stop = false;

/* 
* set non blocking to fd
*/
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/* 
 * add event of fd to the eopllfd
 */
void addfd(int epollfd, int fd, __uint32_t event_add, bool enable_et) {
    epoll_event event;
    event.data.fd = fd;
    event.events = event_add;
    if(enable_et) {
        event.events |= EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

/* 
 * handle the event
 */
void event_handler_et(epoll_event *events, int active_event_num, int epollfd, int listenfd, int *pipefd) {
    char buf[BUFFER_SIZE];
    for(int event_idx = 0; event_idx < active_event_num; ++event_idx) {
        int sockfd = events[event_idx].data.fd;
        __uint32_t cur_event = events[event_idx].events;
        if(sockfd == listenfd) {
            // from server
            if(cur_event & EPOLLRDHUP) {
                printf("[Info] server close the connection.\n");
                m_stop = true;
                break;
            } else if(cur_event & EPOLLIN) {
                memset(read_buf, '\0', BUFFER_SIZE);
                recv(sockfd, read_buf, BUFFER_SIZE-1, 0);
                if(read_buf[strlen(read_buf)-1] == '\n') {
                    read_buf[strlen(read_buf)-1] = '\0';
                }
                printf("%s\n", read_buf);
            }

        } else if(sockfd == 0 && (cur_event & EPOLLIN)) {
            // from client terminal
            int ret = splice( 0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE );
            ret = splice( pipefd[0], NULL, listenfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE );

        } else {
            printf("[Warn] don't know what sockfd: %d\n", sockfd);
        }
    }
}

/* 
 * main function
 */
int main( int argc, char* argv[] )
{
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
        return 1;
    }
    const char* server_ip = argv[1];
    int port = atoi( argv[2] );

    /* 
     * construct socket address of server
     */
    struct sockaddr_in server_address;
    bzero( &server_address, sizeof( server_address ) );
    server_address.sin_family = AF_INET;
    inet_pton( AF_INET, server_ip, &server_address.sin_addr );
    server_address.sin_port = htons( port );

    int sockfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( sockfd >= 0 );
    if ( connect( sockfd, ( struct sockaddr* )&server_address, sizeof( server_address ) ) < 0 )
    {
        printf( "[Error] connection failed\n" );
        close( sockfd );
        return 1;
    } else {
        printf("[Info] connected to the server %s:%d!\n", server_ip, port);
    }

    /* 
     * epoll method
     */
    // create epollfd
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, 0, EPOLLIN, true);
    addfd(epollfd, sockfd, EPOLLIN | EPOLLRDHUP, true);

    // pipe for sending data to sockfd
    int pipefd[2];
    int ret = pipe(pipefd);
    assert(ret != -1);

    while(!m_stop) {
        int active_event_num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(active_event_num < 0) {
            printf("[Error] epoll failure!\n");
            break;
        }

        event_handler_et(events, active_event_num, epollfd, sockfd, pipefd);
    }
    
    close( sockfd );
    return 0;
}
