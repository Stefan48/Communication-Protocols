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
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s <ID_Client> <IP_Server> <Port_Server>\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		usage(argv[0]);
	}
	else if(strlen(argv[1]) > 10)
		exit(0);
	
	int ret;
	int bytes_remaining;
	int bytes_sent;
	int bytes_received;
	
	fd_set read_fds;
	fd_set tmp_fds;
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	char cmd[CMD_LEN];
	char word[CMD_LEN];

	int servfd = socket(PF_INET, SOCK_STREAM, 0);
	DIE(servfd == -1, "socket");
	int enable = 1;
	// disable Nagle's algorithm
	ret = setsockopt(servfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
	DIE(ret == -1, "setsockopt");

	FD_SET(STDIN_FILENO, &read_fds); // for reading from stdin
	FD_SET(servfd, &read_fds); // for receiving messages from server
	int fdmax = servfd;

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	// connect to server
	ret = connect(servfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
	// send connection message to server
	uint16_t len = strlen(argv[1]);
	uint16_t size = len;
	uint16_t size_network = htons(size);
	memcpy(cmd, &size_network, 2);
	memcpy(&cmd[2], argv[1], len);
	bytes_remaining = size + 2;
	bytes_sent = 0;
	while(bytes_remaining > 0)
	{
		ret = send(servfd, &cmd[bytes_sent], bytes_remaining, 0);
		DIE(ret == -1, "send");
		bytes_sent += ret;
		bytes_remaining -= ret;
	}
	
	char msg[MSG_LEN];
	uint32_t ip;
	struct in_addr ip_addr;
	uint16_t port;
	uint8_t type;
	uint8_t sign;
	uint16_t short_value;
	uint32_t value;
	uint8_t exp;
	int nr_digits;
	char digits[10];
	char fl[260]; // float notation string for type 2

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
					fgets(cmd, CMD_LEN - 1, stdin);
					// parse input
					int j = 0;
					len = 0;
					while(j < CMD_LEN && cmd[j] != ' ' && cmd[j] != '\n' && cmd[j] != '\0')
						word[len++] = cmd[j++];
					word[len] = '\0';
					if(strcmp(word, "exit") == 0)
					{
						// send disconnect message to server
						ret = send(servfd, NULL, 0, 0);
						DIE(ret == -1, "send");
						// break
						FD_ZERO(&read_fds);
						FD_ZERO(&tmp_fds);
						shutdown(servfd, 2);
						close(servfd);
						return 0;
					}
					else if(strcmp(word, "subscribe") == 0)
					{
						j++;
						len = 0;
						while(j < CMD_LEN && cmd[j] != ' ' && cmd[j] != '\n' && cmd[j] != '\0')
							word[len++] = cmd[j++];
						word[len] = '\0';
						if(len == 0 || len > 50 || cmd[j] == '\n' || cmd[j] == '\0')
							continue;
						if(j >= (CMD_LEN - 1) || (cmd[j + 1] != '0' && cmd[j + 1] != '1'))
							continue;
						char sf = cmd[j + 1];
						j += 2;
						if(j < CMD_LEN && cmd[j] != '\n' && cmd[j] != '\0')
							continue;
						// send subscribe message to server
						size = len + 2;
						size_network = htons(size);
						memcpy(cmd, &size_network, 2);
						cmd[2] = '1';
						cmd[3] = sf;
						memcpy(&cmd[4], word, len);
						bytes_remaining = size + 2;
						bytes_sent = 0;
						while(bytes_remaining > 0)
						{
							ret = send(servfd, &cmd[bytes_sent], bytes_remaining, 0);
							DIE(ret == -1, "send");
							bytes_sent += ret;
							bytes_remaining -= ret;
						}
						// print
						printf("subscribed %s\n", word);
					}
					else if(strcmp(word, "unsubscribe") == 0)
					{
						j++;
						len = 0;
						while(j < CMD_LEN && cmd[j] != ' ' && cmd[j] != '\n' && cmd[j] != '\0')
							word[len++] = cmd[j++];
						word[len] = '\0';
						if(len == 0 || len > 50 || (cmd[j] != '\n' && cmd[j] != '\0'))
							continue;
						// send unsubscribe message to server
						size = len + 1;
						size_network = htons(size);				
						memcpy(cmd, &size_network, 2);
						cmd[2] = '0';
						memcpy(&cmd[3], word, len);
						bytes_remaining = size + 2;
						bytes_sent = 0;
						while(bytes_remaining > 0)
						{
							ret = send(servfd, &cmd[bytes_sent], bytes_remaining, 0);
							DIE(ret == -1, "send");
							bytes_sent += ret;
							bytes_remaining -= ret;
						}
						// print
						printf("unsubscribed %s\n", word);
					}
					else continue;
				}
				else if(i == servfd)
				{
					// receive message from server
					memset(msg, 0, MSG_LEN);
					bytes_remaining = 2;
					bytes_received = 0;
					while(bytes_remaining > 0)
					{
						ret = recv(servfd, &msg[bytes_received], bytes_remaining, 0);
						DIE(ret == -1, "recv");
						if(ret == 0)
						{
							// server closed connection
							FD_ZERO(&read_fds);
							FD_ZERO(&tmp_fds);
							shutdown(servfd, 2);
							close(servfd);
							return 0;
						}
						bytes_received += ret;
						bytes_remaining -= ret;
					}
					
					memcpy(&size_network, msg, 2);
					size = ntohs(size_network);
					bytes_remaining = size;
					bytes_received = 0;
					while(bytes_remaining > 0)
					{
						ret = recv(servfd, &msg[2 + bytes_received], bytes_remaining, 0);
						DIE(ret == -1, "recv");
						bytes_received += ret;
						bytes_remaining -= ret;
					}
					memcpy(&ip, &msg[2], 4);
					ip_addr.s_addr = ip;
					memcpy(&port, &msg[6], 2);
					port = ntohs(port);
					memset(word, 0, 55);
					memcpy(word, &msg[8], 50);
					memcpy(&type, &msg[58], 1);
					// INT
					if(type == 0)
					{
						memcpy(&sign, &msg[59], 1);
						memcpy(&value, &msg[60], 4);
						int v = ntohl(value);
						if(sign == 1)
							v *= (-1);
						else if(sign)
							continue;
						printf("%s:%hu - %s - INT - %d\n", inet_ntoa(ip_addr), port, word, v);
					}
					// SHORT_REAL
					else if(type == 1)
					{
						memcpy(&short_value, &msg[59], 2);
						short_value = ntohs(short_value);
						float f = (float)short_value / 100.0f;
						printf("%s:%hu - %s - SHORT_REAL - %.2f\n", inet_ntoa(ip_addr), port, word, f);
					}
					// FLOAT
					else if(type == 2)
					{
						memcpy(&sign, &msg[59], 1);
						memcpy(&value, &msg[60], 4);
						memcpy(&exp, &msg[64], 1);
						value = ntohl(value);
						// convert value to string
						len = 0;
						if(value == 0)
						{
							fl[len++] = '0';
						}
						else
						{
							if(sign == 1)
								fl[len++] = '-';
							else if(sign)
								continue;
							nr_digits = 0;
							while(value)
							{
								digits[nr_digits++] = value % 10 + '0';
								value /= 10;
							}
							int d = nr_digits - 1;
							if(nr_digits <= exp)
								fl[len++] = '0';
							else
							{
								for(int j = exp; j < nr_digits; ++j)
									fl[len++] = digits[d--];
							}
							if(exp)
							{
								fl[len++] = '.';
								for(int j = nr_digits; j < exp; ++j)
									fl[len++] = '0';
								while(d >= 0)
									fl[len++] = digits[d--];
								while(fl[len - 1] == '0')
									len--;
								if(fl[len - 1] == '.')
									len--;
							}
						}
						fl[len] = '\0';
						printf("%s:%hu - %s - FLOAT - %s\n", inet_ntoa(ip_addr), port, word, fl);
					}
					// STRING
					else if(type == 3)
					{
						printf("%s:%hu - %s - STRING - %s\n", inet_ntoa(ip_addr), port, word, &msg[59]);
					}
					else continue;
				}
			}
		}
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	shutdown(servfd, 2);
	close(servfd);
	return 0;
}
