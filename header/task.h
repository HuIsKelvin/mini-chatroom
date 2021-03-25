#ifndef TASK_H
#define TASK_H

#include <unistd.h>
#include <sys/epoll.h>
#include <vector>
#include <map>
#include "dataStruct.h"
#include "util.h"

// #define USER_LIMIT 30
// #define BUFFER_SIZE 64

class Task {
public:
    Task() {};
    Task(int epollfd, int sockfd, int listenfd, __uint32_t cur_event, std::map<int, client_data> *users, locker *user_mutex)
    :sockfd(sockfd), listenfd(listenfd), cur_event(cur_event), users(users) {};
    Task(const Task &t);
    ~Task() {};
    // void init();
    void process();
    // void delete();

private:
    int epollfd;
    int sockfd;
    int listenfd;
    __uint32_t cur_event;
    // std::vector<int> *users;
    std::map<int, client_data> *users;
    locker *user_mutex;
};

Task::Task(const Task &t) {
    this->epollfd = t.epollfd;
    this->sockfd = t.sockfd;
    this->listenfd = t.listenfd;
    this->cur_event = t.cur_event;
    this->users = t.users;
    this->user_mutex = t.user_mutex;
}

void Task::process() {
    printf("task begin to process. sockfd:%d\n", this->sockfd);

    if((sockfd == listenfd) && (cur_event & EPOLLIN)) { // a new client want to connect
        struct sockaddr_in client_address;
        socklen_t client_addr_length = sizeof(client_address);
        int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addr_length);
        
        if(connfd < 0) {
            // TODO - use the log function
            printf("[Error] errno is:%d\n", errno);
            return;
        }
        if(users->size() >= USER_LIMIT) {   // too many users
            const char* info = "[Warn] too many users!\n";
            printf("%s", info);
            send(connfd, info, strlen(info), 0);
            close(connfd);  
            return;
        }
        
        // printf("[Info] lock\n");
        // user_mutex->lock();
        client_data cd;
        cd.address = client_address;
        users->insert(std::pair<int, client_data>(connfd, cd));
        // user_mutex->unlock();
        // printf("[Info] unlock\n");
        // char *msg = "a new client connects!";
        // log(LOG_INFO, msg);
        printf("[Info] a new client connects! socket fd:%d\n", connfd);
        addfd(epollfd, connfd, (EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR), true);
        setnonblocking(connfd);

        printf("num of user:");
        printf("%d\n", (int)this->users->size());

    } else if(cur_event & EPOLLERR) {   // error
        printf("[Error] get an error from %d\n", sockfd);
        char errors[100];
        memset(errors, '\0', 100);
        socklen_t length = sizeof(length);
        if(getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0) {
            printf("[Error] get socket option failed!\n");
        }
        return;
    } else if(cur_event & EPOLLRDHUP) { // a client left
        // user_mutex->lock();
        users->erase(sockfd);
        // user_mutex->unlock();
        close(sockfd);
        // char *msg = "a client left.";
        // log(LOG_INFO, msg);
        printf("[Info] a new client left!\n");

    } else if(cur_event & EPOLLIN) {
        // auto it_client_data = users->find(sockfd);
        // int ret = recv(sockfd, it_client_data->second.buf, BUFFER_SIZE-1, 0);
        printf("[Info] try to get data\n");
        char buf[BUFFER_SIZE];
        memset(buf, '\0', BUFFER_SIZE);
        int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
        printf("[Info] get %d bytes of client data %s from %d\n", ret, buf, sockfd);
        
        if(ret < 0) {
            if(errno != EAGAIN) {
                // user_mutex->lock();
                users->erase(sockfd);
                // user_mutex->unlock();
                close(sockfd);
                printf("[Info] something goes wrong. Remove a client. Current %ld client(s).\n", users->size());
            }
        } else if(ret == 0) {
            printf("[Warn] code should not come to here\n");
        } else {
            // user_mutex->lock();
            // notify other active clients
            for(auto it = users->begin(); it != users->end(); ++it) {
                int sockfd_cur = it->first;
                if(sockfd_cur == sockfd) { continue; }
                // ret = send(sockfd_cur, it_client_data->second.buf, BUFFER_SIZE-1, 0);
                ret = send(sockfd_cur, buf, BUFFER_SIZE-1, 0);
            }
            // user_mutex->unlock();
        }
    }
}

#endif // TASK_H