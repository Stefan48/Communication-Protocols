#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
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

void usage(char*file)
{
	fprintf(stderr,"Usage: %s ip_server port_server number\n", file);
	exit(0);
}


int main(int argc, char*argv[])
{
	if (argc != 4)
		usage(argv[0]);
	
	char buffer[256];

	int servfd = socket(PF_INET, SOCK_DGRAM, 0);
	DIE(servfd == -1, "socket");
	
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[2]));
	inet_aton(argv[1], &server_addr.sin_addr);
	
	uint32_t number = 0;
	bool valid = true;
	for(int i = 0; argv[3][i] != '\0'; ++i)
	{
		if(!isdigit(argv[3][i]))
		{
			valid = false;
			break;
		}
		number = number * 10 + (int)(argv[3][i] - '0');
	}
	if(!valid)
		usage(argv[0]);
		
	memset(buffer, 0, 10);
	memcpy(buffer, &number, 4);
	sendto(servfd, buffer, 4, 0, (struct sockaddr*) &server_addr, sizeof(server_addr));
	cout << "Sent value: " << number << "\n";
	
	close(servfd);
	return 0;
}
