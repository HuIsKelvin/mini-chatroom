# Mini Chatroom

简易网络聊天室，基于 C/S 模型，可供多用户在线实时聊天。

## 文件

- client.cpp: 客户端程序，基于`poll` 系统调用。
- server.cpp: 服务端程序，基于线程池和 `poll` 系统调用。
- client_epoll.cpp: 客户端程序，基于`epoll` 系统调用。
- server_epoll.cpp: 服务端程序，基于线程池和 `epoll` 系统调用。

> 客户端只有两个 socket 连接，所以其实基于 `select` 系统调用的效果更好，因为 `epoll` 要多次触发回调，此处仅是为了练手。

## 部署

系统环境 `Ubuntu 20.04`

先编译生成可执行文件。
```bash
make [option]
    - client
    - server
    - client_epoll
    - server_epoll
```

假设服务端的 IP 地址为 `172.64.0.5`，开启的端口为 `8081`。
```bash
# 服务端开启服务
# ./server_epoll IP port
./server_epoll 172.64.0.5 8081

# 客户端连接服务端
# ./client IP port
./client 172.64.0.5 8081
```