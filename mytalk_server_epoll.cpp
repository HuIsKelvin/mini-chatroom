#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <set>
#include <map>
#include <unistd.h>
#include <errno.h>
#include <string.h>
// #include <fcntl.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/epoll.h>
#include <mutex>
#include "header/threadpool.h"
#include "header/locker.h"
#include "header/task.h"
#include "header/util.h"

#define USER_LIMIT 30
#define BUFFER_SIZE 64
#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024

// struct client_data
// {
//     sockaddr_in address;
//     char* write_buf;
//     char buf[ BUFFER_SIZE ];
// };

int main( int argc, char* argv[] )
{
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi( argv[2] );

    int ret = 0;
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );

    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );

    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret != -1 );

    ret = listen( listenfd, 5 );
    assert( ret != -1 );

    // store user info
    Task *users_tasks = new Task[FD_LIMIT];
    // int user_counter = 0;
    // init thread poll
    threadpool<Task> *pool = new threadpool<Task>(7);
    std::map<int, client_data> *users = new std::map<int, client_data>();
    // locker *user_mutex = new locker();
    locker user_mutex;

    // create epoll
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, 0, EPOLLIN | EPOLLERR, true);
    addfd(epollfd, listenfd, EPOLLIN, true);

    while(true) {
        int active_event_num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(active_event_num < 0) {
            // char *msg = "epoll failure!\n";
            // log(LOG_ERROR, msg);
            printf("[Error] epoll failure!\n");
            break;
        }

        for(int event_idx = 0; event_idx < active_event_num; ++event_idx) {
            int sockfd = events[event_idx].data.fd;
            __uint32_t cur_event = events[event_idx].events;
            users_tasks[sockfd] = Task(epollfd, sockfd, listenfd, cur_event, users, &user_mutex);
            pool->append(&users_tasks[sockfd]);

            printf("listenfd:%d,\tsockfd: %d,\tevent:%d\n", listenfd, sockfd, cur_event);

            // if(sockfd == listenfd && (cur_event & EPOLLIN)) {
            //     pri
            // }
        }
    }

    // delete user_mutex;
    // delete [] users;
    delete [] users_tasks;
    delete pool;
    close( listenfd );
    return 0;
}
