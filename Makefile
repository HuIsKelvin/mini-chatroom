client: mytalk_client.cpp
	g++ mytalk_client.cpp -o client.out

client_epoll: mytalk_client_epoll.cpp
	g++ mytalk_client_epoll.cpp -o client_epoll.out

server: mytalk_server.cpp
	g++ mytalk_server.cpp -o server.out