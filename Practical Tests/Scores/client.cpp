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

	string input;
	char buffer[BUFLEN];

	int servfd = socket(PF_INET, SOCK_STREAM, 0);
	DIE(servfd == -1, "socket");

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(8080);
	ret = inet_aton("127.0.0.1", &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");
	
	getline(cin, input);
	for(int i = 0; ; ++i)
	{
		if(input[i] == ' ' || input[i] == '\n' || input[i] == '\0')
		{
			input[i] = '\0';
			break;
		}
		if(!isdigit(input[i]))
		{
			cout << "Invalid id\n";
			return 0;
		}
	}

	// connect to server
	ret = connect(servfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
	// send id to server
	memset(buffer, 0, 15);
	for(int i = 0; input[i] != '\0'; ++i)
		buffer[i] = input[i];
	bytes_remaining = 10;
	bytes_sent = 0;
	while(bytes_remaining > 0)
	{
		ret = send(servfd, &buffer[bytes_sent], bytes_remaining, 0);
		DIE(ret == -1, "recv");
		bytes_received += ret;
		bytes_remaining -= ret;
	}
	
	int score;
	int id;
	
	while(1)
	{
		getline(cin, input);
		if(input == "exit")
			goto exit_client;
		if(strncmp(input.c_str(), "score ", 6) == 0)
		{
			bool valid = true;
			memset(buffer, 0, 15);
			for(int i = 6; input[i] != '\0'; ++i)
			{
				if(!isdigit(input[i]))
				{
					valid = false;
					break;
				}
				buffer[i - 6] = input[i];
			}
			if(valid)
			{
				// send score to server
				bytes_remaining = 10;
				bytes_sent = 0;
				while(bytes_remaining > 0)
				{
					ret = send(servfd, &buffer[bytes_sent], bytes_remaining, 0);
					DIE(ret == -1, "send");
					bytes_sent += ret;
					bytes_remaining -= ret;
				}
			}
			else cout << "Invalid score\n";
			continue;
		}
		if(input == "get score")
		{
			memset(buffer, 'x', 10);
			// send get minimum score request to server
			bytes_remaining = 10;
			bytes_sent = 0;
			while(bytes_remaining > 0)
			{
				ret = send(servfd, &buffer[bytes_sent], bytes_remaining, 0);
				DIE(ret == -1, "send");
				bytes_sent += ret;
				bytes_remaining -= ret;
			}
			// receive reply from server
			// receive minimum score
			memset(buffer, 0, 15);
			bytes_remaining = 10;
			bytes_received = 0;
			while(bytes_remaining > 0)
			{
				ret = recv(servfd, &buffer[bytes_received], bytes_remaining, 0);
				DIE(ret == -1, "recv");
				if(ret == 0)
				{
					// server closed connection
					goto exit_client;
				}
				bytes_received += ret;
				bytes_remaining -= ret;
			}
			//printf("%s\n", buffer);
			score = atoi(buffer);
			// receive min_id
			memset(buffer, 0, 15);
			bytes_remaining = 10;
			bytes_received = 0;
			while(bytes_remaining > 0)
			{
				ret = recv(servfd, &buffer[bytes_received], bytes_remaining, 0);
				DIE(ret == -1, "recv");
				if(ret == 0)
				{
					// server closed connection
					goto exit_client;
				}
				bytes_received += ret;
				bytes_remaining -= ret;
			}
			id = atoi(buffer);
			cout << "Minimum score (" << score << "), submitted by client " << id << "\n";
			continue;
		}
		cout << "Invalid input\n";
	}

exit_client:
	shutdown(servfd, 2);
	close(servfd);
	return 0;
}
