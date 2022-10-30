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
	
	vector<int> clients;
	int original_sender_fd;
	bool original_sender_connected = false;
	
	string input;
	char buffer[BUFLEN];
	uint8_t len;
	
	bool received_word = false;
	char word[BUFLEN];
	char state[BUFLEN];
	int word_len;
	int lives = 5;
	char letter;
	
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
					else continue;
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
					
					//DEBUG
					//cout << "New connection: " << clientfd << "\n";
				}
				else
				{
					// new message from TCP client
					ret = recv(i, &len, 1, 0);
					DIE(ret == -1, "recv");
					if(ret == 0)
					{
						// client closed connection
						shutdown(i, 2);
						close(i);
						FD_CLR(i, &read_fds);
						clients.erase(std::remove(clients.begin(), clients.end(), i), clients.end());
						if(i == original_sender_fd)
							original_sender_connected = false;
					}
					if(ret)
					{
						memset(buffer, 0, BUFLEN);
						bytes_remaining = len;
						bytes_received = 0;
						while(bytes_remaining > 0)
						{
							ret = recv(i, &buffer[bytes_received], bytes_remaining, 0);
							DIE(ret == -1, "recv");
							bytes_received += ret;
							bytes_remaining -= ret;
						}
						
						//DEBUG
						//printf("%s\n", buffer);
						
						if(original_sender_connected && i == original_sender_fd)
						{
							// ignore message from original sender
							continue;
						}
						
						if(!received_word)
						{
							// received the word
							received_word = true;
							strcpy(word, buffer);
							word_len = strlen(word);
							memset(state, '-', BUFLEN);
							state[word_len] = '\0';
							// save word in capital letters
							for(int j = 0; j < word_len; ++j)
								word[j] = toupper(word[j]);
							original_sender_fd = i;
							original_sender_connected = true;
						}
						else
						{
							if(len > 1)
							{
								// invalid message
								memset(buffer, 0, BUFLEN);
								strcpy(&buffer[1], "Input invalid");
								len = strlen(&buffer[1]);
								buffer[0] = len;
								// send reply to client
								bytes_remaining = len + 1;
								bytes_sent = 0;
								while(bytes_remaining > 0)
								{
									ret = send(i, &buffer[bytes_sent], bytes_remaining, 0);
									DIE(ret == -1, "send");
									bytes_sent += ret;
									bytes_remaining -= ret;
								}
							}
							else // if(len == 1)
							{
								// received a letter, convert it to uppercase
								letter = toupper(buffer[0]);
								memset(buffer, 0, BUFLEN);
								bool correct = false;
								for(int j = 0; j < word_len; ++j)
								{
									if(word[j] == letter)
									{
										correct = true;
										state[j] = letter;
									}
								}
								if(correct)
								{
									sprintf(&buffer[1], "Caracterul %c se gaseste in cuvant", letter);
									len = strlen(&buffer[1]);
									buffer[0] = len;
									// send message to all clients except the original sender
									for(int j = 0; j < (int)clients.size(); ++j)
										if(!original_sender_connected || clients[j] != original_sender_fd)
										{
											bytes_remaining = len + 1;
											bytes_sent = 0;
											while(bytes_remaining > 0)
											{
												ret = send(clients[j], &buffer[bytes_sent], bytes_remaining, 0);
												DIE(ret == -1, "send");
												bytes_sent += ret;
												bytes_remaining -= ret;
											}
										}
									// check if clients won
									bool won = true;
									for(int j = 0; j < word_len; ++j)
										if(state[j] == '-')
										{
											won = false;
											break;
										}
									if(won)
									{
										// clients won the game
										memset(buffer, 0, BUFLEN);
										sprintf(&buffer[1], "Ati castigat! Cuvantul este %s", word);
										len = strlen(&buffer[1]);
										buffer[0] = len;
										// send message to all clients except original sender
										for(int j = 0; j < (int)clients.size(); ++j)
											if(!original_sender_connected || clients[j] != original_sender_fd)
											{
												bytes_remaining = len + 1;
												bytes_sent = 0;
												while(bytes_remaining > 0)
												{
													ret = send(clients[j], &buffer[bytes_sent], bytes_remaining, 0);
													DIE(ret == -1, "send");
													bytes_sent += ret;
													bytes_remaining -= ret;
												}
											}
										goto exit_server;
									}
								}
								else
								{
									sprintf(&buffer[1], "Caracterul %c nu se gaseste in cuvant", letter);
									len = strlen(&buffer[1]);
									buffer[0] = len;
									// send message to all clients except the original sender
									for(int j = 0; j < (int)clients.size(); ++j)
										if(!original_sender_connected || clients[j] != original_sender_fd)
										{
											bytes_remaining = len + 1;
											bytes_sent = 0;
											while(bytes_remaining > 0)
											{
												ret = send(clients[j], &buffer[bytes_sent], bytes_remaining, 0);
												DIE(ret == -1, "send");
												bytes_sent += ret;
												bytes_remaining -= ret;
											}
										}
									lives--;
									if(lives == 0)
									{
										// clients lost the game
										memset(buffer, 0, BUFLEN);
										sprintf(&buffer[1], "Ati pierdut! Cuvantul era %s", word);
										len = strlen(&buffer[1]);
										buffer[0] = len;
										// send message to all clients except original sender
										for(int j = 0; j < (int)clients.size(); ++j)
											if(!original_sender_connected || clients[j] != original_sender_fd)
											{
												bytes_remaining = len + 1;
												bytes_sent = 0;
												while(bytes_remaining > 0)
												{
													ret = send(clients[j], &buffer[bytes_sent], bytes_remaining, 0);
													DIE(ret == -1, "send");
													bytes_sent += ret;
													bytes_remaining -= ret;
												}
											}
										goto exit_server;
									}
								}
							}
						}
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
