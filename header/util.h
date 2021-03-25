#ifndef UTIL_H
#define UTIL_H

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string>

enum log_type {LOG_INFO = 1, LOG_WARN, LOG_ERROR};

int setnonblocking(int fd);
void addfd(int epollfd, int fd, __uint32_t newEvent, bool enable_oneshot);
void modfd(int epollfd, int fd, __uint32_t newEvent, bool enable_oneshot);
void removefd(int epollfd, int fd, __uint32_t newEvent, bool enable_oneshot);
void log(int type, char *msg);

void addfd(int epollfd, int fd, __uint32_t newEvent, bool enable_oneshot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = newEvent;
    if(enable_oneshot) {
        // event.events |= EPOLLET | EPOLLONESHOT;
        event.events |= EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}

void modfd(int epollfd, int fd, __uint32_t newEvent, bool enable_oneshot) {

}

void removefd(int epollfd, int fd, __uint32_t newEvent, bool enable_oneshot) {

}

int setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

// void addfd(int epollfd, int fd, __uint32_t event_add, bool enable_et) {
//     epoll_event event;
//     event.data.fd = fd;
//     event.events = event_add;
//     if(enable_et) {
//         event.events |= EPOLLET;
//     }
//     epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
//     setnonblocking(fd);
// }

void log(int type, char *msg) {
    if(type == LOG_INFO) { printf("[Info] "); }
    else if(type == LOG_WARN) { printf("[Warn] "); }
    else if(type == LOG_ERROR) { printf("[Error] "); }
    printf("%s\n", msg);
}

#endif // UTIL_H