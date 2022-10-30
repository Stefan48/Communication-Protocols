#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <bits/stdc++.h>

#define MAX_WAITING_CLIENTS 10

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

using namespace std;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s port\n", file);
	exit(0);
}

void send_msg(int sockfd, char *buffer, int size)
{
	int ret;
	int bytes_remaining = size;
	int bytes_sent = 0;
	while(bytes_remaining > 0)
	{
		ret = send(sockfd, &buffer[bytes_sent], bytes_remaining, 0);
		DIE(ret == -1, "send");
		bytes_sent += ret;
		bytes_remaining -= ret;
	}
}

bool recv_msg(int sockfd, char *buffer, int size)
{
	int ret;
	int bytes_remaining = size;
	int bytes_received = 0;
	while(bytes_remaining > 0)
	{
		ret = recv(sockfd, &buffer[bytes_received], bytes_remaining, 0);
		DIE(ret == -1, "recv");
		if(ret == 0)
		{
			// closed connection
			return false;
		}
		bytes_received += ret;
		bytes_remaining -= ret;
	}
	return true;
}

int main(int argc, char *argv[])
{
	if(argc != 2)
	{
		usage(argv[0]);
	}
	
	fd_set read_fds;
	fd_set tmp_fds;
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	
	socklen_t sockaddr_len = sizeof(struct sockaddr_in);
	struct sockaddr_in my_sockaddr;
	my_sockaddr.sin_family = AF_INET;
	my_sockaddr.sin_port = htons(atoi(argv[1]));
	my_sockaddr.sin_addr.s_addr = INADDR_ANY;
	
	int enable = 1;
	int newsfd = socket(PF_INET, SOCK_DGRAM, 0);
	DIE(newsfd == -1, "socket");
	int ret = setsockopt(newsfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	DIE(ret == -1, "setsockopt");
	ret = bind(newsfd, (struct sockaddr*) &my_sockaddr, sockaddr_len);
	DIE(ret == -1, "bind");
	
	int listenfd = socket(PF_INET, SOCK_STREAM, 0);
	DIE(listenfd == -1, "socket");
	ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	DIE(ret == -1, "setsockopt");
	ret = bind(listenfd, (struct sockaddr*) &my_sockaddr, sockaddr_len);
	DIE(ret == -1, "bind");
	ret = listen(listenfd, MAX_WAITING_CLIENTS);
	DIE(ret == -1, "listen");
	
	FD_SET(STDIN_FILENO, &read_fds); // for reading from stdin
	FD_SET(newsfd, &read_fds); // for receiving news from UDP clients
	FD_SET(listenfd, &read_fds); // for new connections
	int fdmax = max(newsfd, listenfd);
	
	struct sockaddr_in client_addr;
	int clientfd; // for new TCP clients
	
	string input;
	char buffer[256];
	
	vector<int> numbers;
	int sum;
	int avg;
	
	uint32_t list_size;
	uint32_t number;
	
	
	while(1)
	{
		tmp_fds = read_fds;
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");
		for(int i = 0; i <= fdmax; ++i)
		{
			if(FD_ISSET(i, &tmp_fds))
			{
				if(i == STDIN_FILENO)
				{
					// read from stdin
					getline(cin, input);
					if(input == "exit")
						goto exit_server;
					continue;
				}
				else if(i == newsfd)
				{
					// new message from UDP client
					ret = recvfrom(newsfd, buffer, 4, 0, (struct sockaddr*) &client_addr, &sockaddr_len);
					DIE(ret == -1, "recvfrom");
					memcpy(&number, buffer, 4);
					//cout << "Am primit " << number << "\n"; // TODO
					cout << "Received " << number << " from " << inet_ntoa(client_addr.sin_addr) << ":" << client_addr.sin_port << "\n";
					
					// update numbers list and average
					numbers.push_back((int)number);
					sum += (int)number;
					avg = sum / numbers.size();
					
				}
				else if(i == listenfd)
				{
					// new connection
					clientfd = accept(listenfd, (struct sockaddr*) &client_addr, &sockaddr_len);
					DIE(clientfd == -1, "accept");
					FD_SET(clientfd, &read_fds);
					if(clientfd > fdmax)
						fdmax = clientfd;
					// send current stats to client
					list_size = (uint32_t)numbers.size();
					memset(buffer, 0, 10);
					memcpy(buffer, &list_size, 4);
					send_msg(clientfd, buffer, 4);
					for(unsigned j = 0; j < list_size; ++j)
					{
						number = (uint32_t)numbers[j];
						memset(buffer, 0, 10);
						memcpy(buffer, &number, 4);
						send_msg(clientfd, buffer, 4);
					}
					number = (uint32_t)avg;
					memset(buffer, 0, 10);
					memcpy(buffer, &number, 4);
					send_msg(clientfd, buffer, 4);
				}
				else
				{
					// new message from TCP client
					bool ok = recv_msg(i, buffer, 4);
					if(!ok)
					{
						// client closed connection
						shutdown(i, 2);
						close(i);
						FD_CLR(i, &read_fds);
					}
				}
			}
		}
	}
	
exit_server:
	shutdown(newsfd, 2);
	close(newsfd);
	shutdown(listenfd, 2);
	close(listenfd);
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	return 0;
}
