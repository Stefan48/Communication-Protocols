#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <bits/stdc++.h>

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
			// connection closed
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
	
	string input;
	char buffer[BUFLEN];

	int servfd = socket(PF_INET, SOCK_STREAM, 0);
	DIE(servfd == -1, "socket");
	
	FD_SET(STDIN_FILENO, &read_fds); // for reading from stdin
	FD_SET(servfd, &read_fds); // for receiving messages from server
	int fdmax = servfd;

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(8080);
	int ret = inet_aton("127.0.0.1", &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	// connect to server
	ret = connect(servfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
	
	// send connection message to server
	memset(buffer, 0, BUFLEN);
	sprintf(buffer, "hi there");
	send_msg(servfd, buffer, 10);
	
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
					getline(cin, input);
					if(input == "exit")
						goto exit_client;
					//if(input == "smth")
					{
						// send message to server
						memset(buffer, 0, BUFLEN);
						for(int i = 0; input[i] != '\0'; ++i)
							buffer[i] = input[i];
						send_msg(servfd, buffer, 10);
						continue;
					}
					//cout << "Invalid input\n";
				}
				else if(i == servfd)
				{
					// new message from server
					memset(buffer, 0, BUFLEN);
					bool ok = recv_msg(servfd, buffer, 10);
					if(!ok)
					{
						// server closed connection
						goto exit_client;
					}
					printf("%s\n", buffer);
				}
			}
		}
	}

exit_client:
	shutdown(servfd, 2);
	close(servfd);
	return 0;
}
