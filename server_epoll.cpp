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
#define THREAD_NUM 7

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

    // tasks of users
    Task *users_tasks = new Task[FD_LIMIT];
    // init thread poll
    threadpool<Task> *pool = NULL;
    try {
        pool = new threadpool<Task>(THREAD_NUM);
    } catch(...) {
        return 1;
    }
    // client data store
    std::map<int, client_data> *users = new std::map<int, client_data>();
    // mutex for accessing users
    locker user_mutex;

    // create epoll
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(FD_LIMIT);
    assert(epollfd != -1);
    addfd(epollfd, 0, EPOLLIN | EPOLLERR, true);
    addfd(epollfd, listenfd, EPOLLIN, true);

    printf("============================\n");
    printf("  chat room is running!\n");
    printf("============================\n");
    while(true) {
        int active_event_num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(active_event_num < 0) {
            printf("[Error] epoll failure!\n");
            break;
        }

        for(int event_idx = 0; event_idx < active_event_num; ++event_idx) {
            int sockfd = events[event_idx].data.fd;
            __uint32_t cur_event = events[event_idx].events;
            users_tasks[sockfd] = Task(epollfd, sockfd, listenfd, cur_event, users, &user_mutex);

            if(sockfd == listenfd && (cur_event&EPOLLIN)) { // new connection
                struct sockaddr_in client_address;
                socklen_t client_addr_length = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addr_length);  // accept new socket
                client_data cd;
                cd.address = client_address;

                user_mutex.lock();
                users->insert(std::pair<int, client_data>(connfd, cd));
                user_mutex.unlock();
                printf("[Info] a new client connects! socket fd:%d\n", connfd);
                addfd(epollfd, connfd, (EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR), true);
                setnonblocking(connfd);

                char welcome[] = "===============\nWelcome to the chatroom!\n===============";
                send(connfd, welcome, strlen(welcome), 0);

            } else {    // other event, append to the task list
                pool->append(users_tasks + sockfd);
            }
        }
    }

    delete users;
    delete [] users_tasks;
    delete pool;
    close( listenfd );
    return 0;
}
