client: client.cpp
	g++ client.cpp -o client.out

client_epoll: client_epoll.cpp
	g++ client_epoll.cpp -o client_epoll.out

server: server.cpp
	g++ server.cpp -o server.out

server_epoll: server_epoll.cpp
	g++ server_epoll.cpp -o server_epoll.out -lpthread