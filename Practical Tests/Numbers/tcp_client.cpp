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
	fprintf(stderr, "Usage: %s ip_server port_server\n", file);
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
			// connection closed
			return false;
		}
		bytes_received += ret;
		bytes_remaining -= ret;
	}
	return true;
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		usage(argv[0]);
	}
	
	char buffer[256];
	uint32_t list_size;
	uint32_t number;

	int servfd = socket(PF_INET, SOCK_STREAM, 0);
	DIE(servfd == -1, "socket");
	
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[2]));
	int ret = inet_aton(argv[1], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	// connect to server
	ret = connect(servfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
	
	memset(buffer, 0, 10);
	recv_msg(servfd, buffer, 4);
	memcpy(&list_size, buffer, 4);
	if(list_size == 0)
	{
		cout << "List is empty\n";
	}
	else
	{
		cout << "Numbers are: ";
		for(unsigned int i = 0; i < list_size; ++i)
		{
			memset(buffer, 0, 10);
			recv_msg(servfd, buffer, 4);
			memcpy(&number, buffer, 4);
			cout << number << " ";
		}
		memset(buffer, 0, 10);
		recv_msg(servfd, buffer, 4);
		memcpy(&number, buffer, 4);
		cout << "\nAverage is: " << number << "\n";
	}

	shutdown(servfd, 2);
	close(servfd);
	return 0;
}
