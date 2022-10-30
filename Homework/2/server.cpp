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
#include "helpers.h"

#define MAX_WAITING_CLIENTS 10

using namespace std;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s <Port>\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		usage(argv[0]);
	}
	
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
	my_sockaddr.sin_port = htons(atoi(argv[1]));
	my_sockaddr.sin_addr.s_addr = INADDR_ANY;
	
	int enable = 1;
	int newsfd = socket(PF_INET, SOCK_DGRAM, 0);
	DIE(newsfd == -1, "socket");
	ret = setsockopt(newsfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	DIE(ret == -1, "setsockopt");
	ret = bind(newsfd, (struct sockaddr*) &my_sockaddr, sockaddr_len);
	DIE(ret == -1, "bind");
	
	int listenfd = socket(PF_INET, SOCK_STREAM, 0);
	DIE(listenfd == -1, "socket");
	ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	DIE(ret == -1, "setsockopt");
	ret = bind(listenfd, (struct sockaddr*) &my_sockaddr, sockaddr_len);
	DIE(ret == -1, "bind");
	ret = listen(listenfd, MAX_WAITING_CLIENTS);
	DIE(ret == -1, "listen");
	
	FD_SET(STDIN_FILENO, &read_fds); // for reading from stdin
	FD_SET(newsfd, &read_fds); // for receiving news from UDP clients
	FD_SET(listenfd, &read_fds); // for new connections
	int fdmax = max(newsfd, listenfd);
	
	struct sockaddr_in client_addr;
	int clientfd; // for new TCP clients
	
	uint16_t len;
	uint16_t size;
	uint16_t size_network;
	char cmd[CMD_LEN];
	char word[CMD_LEN];
	char msg[MSG_LEN];
	char msg2[MSG_LEN];
	vector<char> msg_vector;
	string topic;
	string client_id;
	uint8_t type;
	
	unordered_map<int, string> socket_id_map; // key == socket, value == id
	unordered_map<string, int> id_socket_map; // key == id, value == socket
	unordered_map<string, vector< pair<string, char> > > subscribers; // subscribers for each topic
	unordered_map<string, vector< vector<char> > > to_be_sent; // messages to be sent for each client
	unordered_set<string> connected; // currently connected clients
	
	vector< pair<string, char> > subs;
	pair<string, char> sub;
	vector< vector<char> > tbs;
	
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
					fgets(cmd, CMD_LEN - 1, stdin);
					// parse input
					len = 0;
					while(len < 5 && cmd[len] != '\n' && cmd[len] != '\0')
					{
						word[len] = cmd[len];
						len++;
					}
					word[len] = '\0';
					if(strcmp(word, "exit") == 0)
					{
						// close connections with clients
						for(auto it = socket_id_map.begin(); it != socket_id_map.end(); ++it)
						{
							ret = send(it->first, NULL, 0, 0);
							DIE(ret == -1, "send");
							shutdown(it->first, 2);
							close(it->first);
						}
						// break
						shutdown(newsfd, 2);
						close(newsfd);
						shutdown(listenfd, 2);
						close(listenfd);
						FD_ZERO(&read_fds);
						FD_ZERO(&tmp_fds);
						return 0;
					}
					else continue;
				}
				else if(i == newsfd)
				{
					// new message from UDP client
					ret = recvfrom(newsfd, msg, MSG_LEN, 0, (struct sockaddr*) &client_addr, &sockaddr_len);
					DIE(ret == -1, "recvfrom");
					size = (uint16_t)ret;
					if(size < 52 || size > 1551)
						continue;
					memcpy(&type, &msg[50], 1);
					if((type == 0 && size < 56) || (type == 1 && size < 53) || (type == 2 && size < 57))
						continue;
					memset(word, 0, 55);
					memcpy(word, msg, 50);
					topic = word;
					// add source's ip and port to message
					size_network = htons(size + 6);
					memcpy(msg2, &size_network, 2);
					memcpy(&msg2[2], &client_addr.sin_addr.s_addr, 4);
					memcpy(&msg2[6], &client_addr.sin_port, 2);
					memcpy(&msg2[8], msg, size);
					// search for subscribers to this topic
					if(subscribers.find(topic) != subscribers.end())
					{
						subs = subscribers[topic];
						if(!subs.empty())
						{
							for(auto it = subs.begin(); it != subs.end(); ++it)
							{
								sub = *it;
								if(connected.find(sub.first) != connected.end())
								{
									// client is currently connected, send message
									clientfd = id_socket_map[sub.first];
									bytes_remaining = size + 8;
									bytes_sent = 0;
									while(bytes_remaining > 0)
									{
										ret = send(clientfd, &msg2[bytes_sent], bytes_remaining, 0);
										DIE(ret == -1, "send");
										bytes_sent += ret;
										bytes_remaining -= ret;
									}
								}
								else if(sub.second == '1')
								{
									// client is not currently connected, but SF == 1
									msg_vector.clear();
									for(int j = 0; j < size + 8; ++j)
										msg_vector.push_back(msg2[j]);
									tbs = to_be_sent[sub.first];
									tbs.push_back(msg_vector);
									to_be_sent[sub.first] = tbs;
								}
							}
						}
					}
				}
				else if(i == listenfd)
				{
					// new connection
					clientfd = accept(listenfd, (struct sockaddr*) &client_addr, &sockaddr_len);
					DIE(clientfd == -1, "accept");
					// disable Nagle's algorithm
					ret = setsockopt(clientfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
					DIE(ret == -1, "setsockopt");
					FD_SET(clientfd, &read_fds);
					if(clientfd > fdmax)
						fdmax = clientfd;
					// receive connection message (to get Client_Id)
					memset(cmd, 0, 15);
					bytes_remaining = 2;
					bytes_received = 0;
					while(bytes_remaining > 0)
					{
						ret = recv(clientfd, &cmd[bytes_received], bytes_remaining, 0);
						DIE(ret == -1, "recv");
						bytes_received += ret;
						bytes_remaining -= ret;
					}
					
					memcpy(&size_network, cmd, 2);
					size = ntohs(size_network);
					bytes_remaining = size;
					bytes_received = 0;
					while(bytes_remaining > 0)
					{
						ret = recv(clientfd, &cmd[2 + bytes_received], bytes_remaining, 0);
						DIE(ret == -1, "recv");
						bytes_received += ret;
						bytes_remaining -= ret;
					}
					
					client_id = &cmd[2];
					if(connected.find(client_id) != connected.end())
					{
						// another client with the same id is already connected
						ret = send(clientfd, NULL, 0, 0);
						DIE(ret == -1, "send");
						shutdown(clientfd, 2);
						close(clientfd);
						FD_CLR(clientfd, &read_fds);
					}
					else
					{
						socket_id_map[clientfd] = client_id;
						id_socket_map[client_id] = clientfd;
						connected.insert(client_id);
						// print
						printf("New client (%s) connected from %s:%hu.\n", &cmd[2], inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
						tbs = to_be_sent[client_id];
						if(!tbs.empty())
						{
							// send messages lost by the client while offline
							for(auto it = tbs.begin(); it != tbs.end(); ++it)
							{
								msg_vector = *it;
								for(unsigned int j = 0; j < msg_vector.size(); ++j)
									msg[j] = msg_vector[j];
								bytes_remaining = msg_vector.size();
								bytes_sent = 0;
								while(bytes_remaining > 0)
								{
									ret = send(clientfd, &msg[bytes_sent], bytes_remaining, 0);
									DIE(ret == -1, "send");
									bytes_sent += ret;
									bytes_remaining -= ret;
								}
							}
						}
						to_be_sent.erase(client_id);
					}
				}
				else
				{
					// new message from TCP client
					client_id = socket_id_map[i];
					bytes_remaining = 2;
					bytes_received = 0;
					while(bytes_remaining > 0)
					{
						ret = recv(i, &cmd[bytes_received], bytes_remaining, 0);
						DIE(ret == -1, "recv");
						if(ret == 0)
						{
							// client closed connection
							socket_id_map.erase(i);
							id_socket_map.erase(client_id);
							connected.erase(client_id);
							shutdown(i, 2);
							close(i);
							FD_CLR(i, &read_fds);
							//print
							cout << "Client (" << client_id << ") disconnected.\n";
							break;
						}
						bytes_received += ret;
						bytes_remaining -= ret;
					}
					if(ret)
					{
						memcpy(&size_network, cmd, 2);
						size = ntohs(size_network);
						bytes_remaining = size;
						bytes_received = 0;
						while(bytes_remaining > 0)
						{
							ret = recv(i, &cmd[2 + bytes_received], bytes_remaining, 0);
							DIE(ret == -1, "recv");
							bytes_received += ret;
							bytes_remaining -= ret;
						}
						
						if(cmd[2] == '1')
						{
							// subscribe message
							char sf = cmd[3];
							memset(word, 0, 55);
							memcpy(word, &cmd[4], size - 2);
							topic = word;
							if(subscribers.find(topic) != subscribers.end())
							{
								subs = subscribers[topic];
								bool found = false;
								for(unsigned int j = 0; j < subs.size(); ++j)
								{
									sub = subs[j];
									if(sub.first == client_id)
									{
										found = true;
										sub.second = sf;
										subs[j] = sub;
										break;
									}
								}
								if(!found)
								{
									subs.push_back({client_id, sf});
								}
								subscribers[topic] = subs;
							}
							else
							{
								subs.clear();
								subs.push_back({client_id, sf});
								subscribers[topic] = subs;
							}
						}
						else if(cmd[2] == '0')
						{
							// unsubscribe message
							memset(word, 0, 55);
							memcpy(word, &cmd[3], size - 1);
							topic = word;
							if(subscribers.find(topic) != subscribers.end())
							{
								subs = subscribers[topic];
								for(unsigned int j = 0; j < subs.size(); ++j)
								{
									sub = subs[j];
									if(sub.first == client_id)
									{
										subs.erase(subs.begin() + j);
										if(subs.empty())
										{
											subscribers.erase(topic);
										}
										else subscribers[topic] = subs;
										break;
									}
								}
							}
						}
						else continue;
					}
				}
			}
		}
	}
	
	// close connections with clients
	for(auto it = socket_id_map.begin(); it != socket_id_map.end(); ++it)
	{
		ret = send(it->first, NULL, 0, 0);
		DIE(ret == -1, "send");
		shutdown(it->first, 2);
		close(it->first);
	}
	shutdown(newsfd, 2);
	close(newsfd);
	shutdown(listenfd, 2);
	close(listenfd);
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	return 0;
}
