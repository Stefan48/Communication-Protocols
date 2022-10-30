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
	ret = inet_aton("127.0.0.1", &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	// connect to server
	ret = connect(servfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
	// send connection message to server
	uint8_t len;

	while (1)
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
					// parse input
					for(int j = 0; ; ++j)
					{
						if(input[j] == ' ' || input[j] == '\n' || input[j] == '\0')
						{
							input[j] = '\0';
							break;
						} 
					}
					len = strlen(input.c_str());
					memset(buffer, 0, BUFLEN);
					buffer[0] = len;
					for(int j = 1; j <= len; ++j)
						buffer[j] = input[j - 1];
					bytes_remaining = len + 1;
					bytes_sent = 0;
					while(bytes_remaining > 0)
					{
						ret = send(servfd, &buffer[bytes_sent], bytes_remaining, 0);
						DIE(ret == -1, "send");
						bytes_sent += ret;
						bytes_remaining -= ret;
					}
					
				}
				else if(i == servfd)
				{
					// receive message from server
					memset(buffer, 0, BUFLEN);
					ret = recv(servfd, &len, 1, 0);
					DIE(ret == -1, "recv");
					if(ret == 0)
					{
						// server closed connection
						goto exit_client;
					}
					bytes_remaining = len;
					bytes_received = 0;
					while(bytes_remaining > 0)
					{
						ret = recv(servfd, &buffer[bytes_received], bytes_remaining, 0);
						DIE(ret == -1, "recv");
						bytes_received += ret;
						bytes_remaining -= ret;
					}
					printf("%s\n", buffer);
					// game end condition
					if(strncmp(buffer, "Ati ", 4) == 0)
					{
						goto exit_client;
					}
				}
			}
		}
	}

exit_client:
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	shutdown(servfd, 2);
	close(servfd);
	return 0;
}
