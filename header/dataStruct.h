#ifndef DATASTRUCT_H
#define DATASTRUCT_H

// #include <sys/un.h>
// #include <sys/socket.h>

#define USER_LIMIT 30
#define BUFFER_SIZE 64
#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024

struct client_data
{
    sockaddr_in address;
    char* write_buf;
    char buf[ BUFFER_SIZE ];
};


#endif // DATASTRUCT_H