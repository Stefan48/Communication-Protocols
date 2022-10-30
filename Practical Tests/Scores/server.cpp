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
#define BUFLEN 256

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


int main()
{
	int ret;
	int bytes_remaining;
	int bytes_sent;
	int bytes_received;
	
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
	ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
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
	
	int id;
	int score;
	int min_score = INT_MAX;
	int min_id = -1;
	map<int, int> socket_id_map;
	
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
					continue;
				}
				else if(i == listenfd)
				{
					// new connection
					clientfd = accept(listenfd, (struct sockaddr*) &client_addr, &sockaddr_len);
					DIE(clientfd == -1, "accept");
					FD_SET(clientfd, &read_fds);
					if(clientfd > fdmax)
						fdmax = clientfd;
					// receive id from client
					memset(buffer, 0, 15);
					bytes_remaining = 10;
					bytes_received = 0;
					while(bytes_remaining > 0)
					{
						ret = recv(clientfd, &buffer[bytes_received], bytes_remaining, 0);
						DIE(ret == -1, "recv");
						bytes_received += ret;
						bytes_remaining -= ret;
					}
					id = atoi(buffer);
					socket_id_map[clientfd] = id;
					if(min_id == -1)
						min_id = id;
				}
				else
				{
					// new message from TCP client
					memset(buffer, 0, BUFLEN);
					bytes_remaining = 10;
					bytes_received = 0;
					while(bytes_remaining > 0)
					{
						ret = recv(i, &buffer[bytes_received], bytes_remaining, 0);
						DIE(ret == -1, "recv");
						if(ret == 0)
						{
							// client closed connection
							shutdown(i, 2);
							close(i);
							FD_CLR(i, &read_fds);
							socket_id_map.erase(i);
							break;
						}
						bytes_received += ret;
						bytes_remaining -= ret;
					}
					if(ret == 0)
						continue;
					
					if(buffer[0] == 'x')
					{
						// got min score request from client, send reply
						// send minimum score
						memset(buffer, 0, 15);
						sprintf(buffer, "%u", min_score);
						bytes_remaining = 10;
						bytes_sent = 0;
						while(bytes_remaining > 0)
						{
							ret = send(i, &buffer[bytes_sent], bytes_remaining, 0);
							DIE(ret == -1, "send");
							bytes_sent += ret;
							bytes_remaining -= ret;
						}
						// send id
						memset(buffer, 0, 15);
						sprintf(buffer, "%u", min_id);
						bytes_remaining = 10;
						bytes_sent = 0;
						while(bytes_remaining > 0)
						{
							ret = send(i, &buffer[bytes_sent], bytes_remaining, 0);
							DIE(ret == -1, "send");
							bytes_sent += ret;
							bytes_remaining -= ret;
						}
					}
					else
					{
						// new score from client
						score = atoi(buffer);
						if(score < min_score)
						{
							min_score = score;
							min_id = socket_id_map[i];
						}
					}
					continue;	
				}
			}
		}
	}
	
exit_server:
	// close connections with clients
	for(auto it = socket_id_map.begin(); it != socket_id_map.end(); ++it)
	{
		ret = send(it->first, NULL, 0, 0);
		DIE(ret == -1, "send");
		shutdown(it->first, 2);
		close(it->first);
	}
	shutdown(listenfd, 2);
	close(listenfd);
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	return 0;
}
