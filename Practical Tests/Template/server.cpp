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
#define BUFLEN 1024

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

int main()
{
	fd_set read_fds;
	fd_set tmp_fds;
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	
	socklen_t sockaddr_len = sizeof(struct sockaddr_in);
	struct sockaddr_in my_sockaddr;
	my_sockaddr.sin_family = AF_INET;
	my_sockaddr.sin_port = htons(8080);
	my_sockaddr.sin_addr.s_addr = INADDR_ANY;
	
	int enable = 1;
	int listenfd = socket(PF_INET, SOCK_STREAM, 0);
	DIE(listenfd == -1, "socket");
	int ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	DIE(ret == -1, "setsockopt");
	ret = bind(listenfd, (struct sockaddr*) &my_sockaddr, sockaddr_len);
	DIE(ret == -1, "bind");
	ret = listen(listenfd, MAX_WAITING_CLIENTS);
	DIE(ret == -1, "listen");
	
	FD_SET(STDIN_FILENO, &read_fds); // for reading from stdin
	FD_SET(listenfd, &read_fds); // for new connections
	int fdmax = listenfd;
	
	struct sockaddr_in client_addr;
	int clientfd; // for new TCP clients
	vector<int> clients; // all connected clients
	
	string input;
	char buffer[BUFLEN];
	
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
					{
						goto exit_server;
					}
					// cout << "Invalid input\n";
				}
				else if(i == listenfd)
				{
					// new connection
					clientfd = accept(listenfd, (struct sockaddr*) &client_addr, &sockaddr_len);
					DIE(clientfd == -1, "accept");
					FD_SET(clientfd, &read_fds);
					if(clientfd > fdmax)
						fdmax = clientfd;
					clients.push_back(clientfd);
					
					// send welcome message
					memset(buffer, 0, BUFLEN);
					sprintf(buffer, "welcome");
					send_msg(clientfd, buffer, 10);
				}
				else
				{
					// new message from TCP client
					memset(buffer, 0, BUFLEN);
					bool ok = recv_msg(i, buffer, 10);
					if(!ok)
					{
						// client disconnected
						shutdown(i, 2);
						close(i);
						FD_CLR(i, &read_fds);
						clients.erase(std::remove(clients.begin(), clients.end(), i), clients.end());
						continue;
					}
					else
					{
						// print received message and send it back
						printf("%s\n", buffer);
						send_msg(i, buffer, 10);
					}
				}
			}
		}
	}
	
exit_server:
	// close connections with clients
	for(auto it = clients.begin(); it != clients.end(); ++it)
	{
		ret = send(*it, NULL, 0, 0);
		DIE(ret == -1, "send");
		shutdown(*it, 2);
		close(*it);
	}
	shutdown(listenfd, 2);
	close(listenfd);
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	return 0;
}
