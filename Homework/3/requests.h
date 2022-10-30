#ifndef _REQUESTS_
#define _REQUESTS_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>

#define BUFLEN 4096

#define DIE(assertion, call_description)    \
    do {                                    \
        if (assertion) {                    \
            fprintf(stderr, "(%s, %d): ",   \
                    __FILE__, __LINE__);    \
            perror(call_description);       \
            exit(EXIT_FAILURE);             \
        }                                   \
    } while(0)

// receives a name and returns a corresponding IP address
std::string get_ip(const char *name);

// adds a line to a string message
std::string add_to_message(std::string message, std::string line);

// computes and returns a GET request string (query_params, cookie and token can be set to "\0" if not needed)
std::string compute_get_request(std::string host, std::string url, std::string query_params, std::string cookie, std::string token);

// computes and returns a POST request string (cookies and token can be set to "\0" if not needed)
std::string compute_post_request(std::string host, std::string url, std::string content_type, std::string data, std::string cookie, std::string token);

// computes and returns a DELETE request string (token can be "\0" if not needed)
std::string compute_delete_request(std::string host, std::string url, std::string token);

// sends message to server and receives corresponding reply
void send_and_receive(int &servfd, struct sockaddr_in &serv_addr, std::string message, char *buffer);

#endif

