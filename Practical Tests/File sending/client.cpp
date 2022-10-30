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

#define BUFLEN 512

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


int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		cout << "Usage: ./client <ID_Client>\n";
		return 0;
	}
	
	char buffer[BUFLEN];

	int servfd = socket(PF_INET, SOCK_STREAM, 0);
	DIE(servfd == -1, "socket");

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(8080);
	int ret = inet_aton("127.0.0.1", &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	// connect to server
	ret = connect(servfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
	
	// open file
	string file_name = argv[1];
	file_name += ".txt";
	//FILE *in = fopen("file.txt", "r");
	FILE *in = fopen(file_name.c_str(), "r");
	DIE(in == NULL, "fopen");
	int s;
	uint8_t size;
	size_t max_size = 255;
	char *start = buffer + 1;
	
	// send file name message to server
	memset(buffer, 0, BUFLEN);
	strcpy(&buffer[1], argv[1]);
	size = strlen(&buffer[1]);
	memcpy(buffer, &size, 1);
	send_msg(servfd, buffer, size + 1);
	// send file content to server
	while((s = getline(&start, &max_size, in)) != -1)
	{
		size = (uint8_t)s;
		memcpy(buffer, &size, 1);
		send_msg(servfd, buffer, (int)size + 1);
	}

//exit_client:
	shutdown(servfd, 2);
	close(servfd);
	fclose(in);
	return 0;
}
