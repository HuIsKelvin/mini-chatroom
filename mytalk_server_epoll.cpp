#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <set>
// #include <map>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/epoll.h>

#define USER_LIMIT 30
#define BUFFER_SIZE 64
#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024

std::set<int> users_set;
// std::map<int, int> users_set;

struct client_data
{
    sockaddr_in address;
    char* write_buf;
    char buf[ BUFFER_SIZE ];
};

int setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

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
    client_data* users = new client_data[FD_LIMIT];
    int user_counter = 0;

    // create epoll
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, 0, EPOLLIN | EPOLLERR, true);
    // for(int i = 0; i < USER_LIMIT; ++i) {
    //     addfd(epollfd, i, EPOLLIN, true);
    // }
    addfd(epollfd, listenfd, EPOLLIN, true);

    while(true) {
        int active_event_num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(active_event_num < 0) {
            printf("[Error] epoll failure!\n");
            break;
        }

        for(int event_idx = 0; event_idx < active_event_num; ++event_idx) {
            int sockfd = events[event_idx].data.fd;
            __uint32_t cur_event = events[event_idx].events;

            if((sockfd == listenfd) && (cur_event & EPOLLIN)) {
                struct sockaddr_in client_address;
                socklen_t client_addr_length = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addr_length);  // accept socket
                if(connfd < 0) {                    // error at accepting socket
                    printf("[Error] errno is:%d\n", errno);
                    continue;
                }
                if(user_counter >= USER_LIMIT) {    // too many users
                    const char* info = "[Warn] too many users!\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }

                users_set.insert(connfd);
                // users_set.insert(std::pair<int, int>(connfd, 1));
                printf("[Info] a new client connect! Current %ld client(s).\n", users_set.size());
                ++user_counter;
                users[connfd].address = client_address;
                setnonblocking(connfd);
                addfd(epollfd, connfd, (EPOLLIN | EPOLLRDHUP | EPOLLERR), true);

            } else if(cur_event & EPOLLERR) {   // something error at getting socket
                printf("[Error] get an error from %d\n", sockfd);
                char errors[100];
                memset(errors, '\0', 100);
                socklen_t length = sizeof(length);
                if(getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0) {
                    printf("[Error] get socket option failed!\n");
                }
                continue;

            } else if(cur_event & EPOLLRDHUP) { // a client left
                users_set.erase(sockfd);
                --user_counter;
                close(sockfd);
                printf("[Info] a client left. Current %ld client(s).\n", users_set.size());

            } else if(cur_event & EPOLLIN) {
                memset(users[sockfd].buf, '\0', BUFFER_SIZE);
                ret = recv(sockfd, users[sockfd].buf, BUFFER_SIZE-1, 0);
                printf("[Info] get %d bytes of client data %s from %d\n", ret, users[sockfd].buf, sockfd);
                if(ret < 0) {
                    if(errno != EAGAIN) {
                        close(sockfd);
                        users_set.erase(sockfd);
                        --user_counter;
                        printf("[Info] something goes wrong. Remove a client. Current %ld client(s).\n", users_set.size());
                    }
                } else if(ret == 0) {
                    printf("[Warn] code should not come to here\n");
                } else {
                    // notify other active clients
                    for(auto it = users_set.begin(); it != users_set.end(); ++it) {
                        int sockfd_cur = *it;
                        if(sockfd_cur == sockfd) { continue; }
                        printf("[Info] traverse sockfd %d\n", sockfd_cur);

                        ret = send(sockfd_cur, users[sockfd].buf, BUFFER_SIZE-1, 0);
                        // addfd(epollfd, sockfd_cur, (~EPOLLIN | EPOLLOUT), true);
                        // users[sockfd_cur].write_buf = users[sockfd].buf;
                        // ret = send(sockfd_cur, users[sockfd_cur].write_buf, strlen(users[sockfd_cur].write_buf), 0);
                        // users[sockfd].write_buf = NULL;
                        // addfd(epollfd, sockfd, (~EPOLLOUT | EPOLLIN), true);
                    }
                }

            // } else if(cur_event & EPOLLOUT) {       // send msg to sockfd
            //     printf("[Info] send msg to sockfd %d\n", sockfd);
            //     if(!users[sockfd].write_buf) {
            //         // addfd(epollfd, sockfd, EPOLLIN, true);
            //         continue;
            //     }
            //     ret = send(sockfd, users[sockfd].write_buf, strlen(users[sockfd].write_buf), 0);
            //     users[sockfd].write_buf = NULL;
            //     addfd(epollfd, sockfd, (~EPOLLOUT | EPOLLIN), true);
            // }
        }
    }

    // while( 1 )
    // {
    //     ret = poll( fds, user_counter+1, -1 );
    //     if ( ret < 0 )
    //     {
    //         printf( "poll failure\n" );
    //         break;
    //     }
    
    //     for( int i = 0; i < user_counter+1; ++i )
    //     {
    //         if( ( fds[i].fd == listenfd ) && ( fds[i].revents & POLLIN ) )
    //         {
    //             struct sockaddr_in client_address;
    //             socklen_t client_addrlength = sizeof( client_address );
    //             int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
    //             if ( connfd < 0 )
    //             {
    //                 printf( "errno is: %d\n", errno );
    //                 continue;
    //             }
    //             if( user_counter >= USER_LIMIT )
    //             {
    //                 const char* info = "too many users\n";
    //                 printf( "%s", info );
    //                 send( connfd, info, strlen( info ), 0 );
    //                 close( connfd );
    //                 continue;
    //             }
    //             user_counter++;
    //             users[connfd].address = client_address;
    //             setnonblocking( connfd );
    //             fds[user_counter].fd = connfd;
    //             fds[user_counter].events = POLLIN | POLLRDHUP | POLLERR;
    //             fds[user_counter].revents = 0;
    //             printf( "comes a new user, now have %d users\n", user_counter );
    //         }
    //         else if( fds[i].revents & POLLERR )
    //         {
    //             printf( "get an error from %d\n", fds[i].fd );
    //             char errors[ 100 ];
    //             memset( errors, '\0', 100 );
    //             socklen_t length = sizeof( errors );
    //             if( getsockopt( fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length ) < 0 )
    //             {
    //                 printf( "get socket option failed\n" );
    //             }
    //             continue;
    //         }
    //         else if( fds[i].revents & POLLRDHUP )   // clinet left
    //         {
    //             // users[fds[i].fd] = users[fds[user_counter].fd];
    //             // close( fds[i].fd );
    //             // fds[i] = fds[user_counter];
    //             close(sockfd);
    //             users_set.erase(sockfd);
    //             i--;
    //             user_counter--;
    //             printf( "a client left\n" );
    //         }
    //         else if( fds[i].revents & POLLIN )
    //         {
    //             int connfd = fds[i].fd;
    //             memset( users[connfd].buf, '\0', BUFFER_SIZE );
    //             ret = recv( connfd, users[connfd].buf, BUFFER_SIZE-1, 0 );
    //             printf( "get %d bytes of client data %s from %d\n", ret, users[connfd].buf, connfd );
    //             if( ret < 0 )
    //             {
    //                 if( errno != EAGAIN )
    //                 {
    //                     close( connfd );
    //                     users[fds[i].fd] = users[fds[user_counter].fd];
    //                     fds[i] = fds[user_counter];
    //                     i--;
    //                     user_counter--;
    //                 }
    //             }
    //             else if( ret == 0 )
    //             {
    //                 printf( "code should not come to here\n" );
    //             }
    //             else
    //             {
    //                 for( int j = 1; j <= user_counter; ++j )
    //                 {
    //                     if( fds[j].fd == connfd )
    //                     {
    //                         continue;
    //                     }
                        
    //                     fds[j].events |= ~POLLIN;
    //                     fds[j].events |= POLLOUT;
    //                     users[fds[j].fd].write_buf = users[connfd].buf;
    //                 }
    //             }
    //         }
    //         else if( fds[i].revents & POLLOUT )
    //         {
    //             int connfd = fds[i].fd;
    //             if( ! users[connfd].write_buf )
    //             {
    //                 continue;
    //             }
    //             ret = send( connfd, users[connfd].write_buf, strlen( users[connfd].write_buf ), 0 );
    //             users[connfd].write_buf = NULL;
    //             fds[i].events |= ~POLLOUT;
    //             fds[i].events |= POLLIN;
    //         }
    //     }
    // }

    delete [] users;
    close( listenfd );
    return 0;
}
