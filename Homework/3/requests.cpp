#include "requests.h"

using namespace std;

string get_ip(const char *name)
{
    struct addrinfo hints, *result, *p;
    // set hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_protocol = IPPROTO_NONE;
    // get addresses
    int ret = getaddrinfo(name, NULL, &hints, &result);
    if(ret < 0)
        gai_strerror(ret);
    // return first (valid) address
    char ip[INET_ADDRSTRLEN];
    struct sockaddr_in *addr;
    string ip_str;
    for(p = result; p != NULL; p = p->ai_next)
    {
        addr = (struct sockaddr_in*)p->ai_addr;
        if(inet_ntop(AF_INET, &(addr->sin_addr), ip, sizeof(ip)) != NULL)
        {
            ip_str = ip;
            break;
        }
    }
    // free allocated data
    freeaddrinfo(result);
    return ip_str;
}

string add_to_message(string message, string line)
{
    return message + line + "\r\n";
}

string compute_get_request(string host, string url, string query_params, string cookie, string token)
{
    string message, line;
    
    if(query_params[0] != '\0')
        line = "GET " + url + "?" + query_params + " HTTP/1.1";
    else line = "GET " + url + " HTTP/1.1";
    message = add_to_message(message, line);
    line = "Host: " + host;
    message = add_to_message(message, line);
    if(cookie[0] != '\0')
    {
        line = "Cookie: " + cookie;
        message = add_to_message(message, line);
    }
    if(token[0] != '\0')
    {
        line = "Authorization: Bearer " + token;
        message = add_to_message(message, line);
    }
    message = add_to_message(message, "");
    return message;
}

string compute_post_request(string host, string url, string content_type, string data, string cookie, string token)
{
    string message, line;
    char content_length[50];
    
    line = "POST " + url + " HTTP/1.1";
    message = add_to_message(message, line);
    line = "Host: " + host;
    message = add_to_message(message, line);
    line = "Content-Type: " + content_type + " ";
    message = add_to_message(message, line);
    sprintf(content_length, "Content-Length: %ld", data.length());
    line = content_length;
    message = add_to_message(message, line);
    if(cookie[0] != '\0')
    {
        line = "Cookie: " + cookie;
        message = add_to_message(message, line);
    }
    if(token[0] != '\0')
    {
        line = "Authorization: Bearer " + token;
        message = add_to_message(message, line);
    }
    message = add_to_message(message, "");
    message = add_to_message(message, data);
    return message;
}

string compute_delete_request(string host, string url, string token)
{
    string message, line;
    
    line = "DELETE " + url + " HTTP/1.1";
    message = add_to_message(message, line);
    line = "Host: " + host;
    message = add_to_message(message, line);
    if(token[0] != '\0')
    {
        line = "Authorization: Bearer " + token;
        message = add_to_message(message, line);
    }
    message = add_to_message(message, "");
    return message;
}

void send_and_receive(int &servfd, struct sockaddr_in &serv_addr, string message, char *buffer)
{
    if(recv(servfd, NULL, 1, MSG_PEEK | MSG_DONTWAIT) == 0)
    {
        // connection down, fix it
        servfd = socket(PF_INET, SOCK_STREAM, 0);
        int ret = connect(servfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
        DIE(ret == -1,  "connect");
    }
    
    int bytes, bytes_sent, bytes_received;
    // send message to server
    memset(buffer, 0, BUFLEN);
    strcpy(buffer, message.c_str());
    int size = strlen(buffer);
    bytes_sent = 0;
    while(bytes_sent < size)
    {
        bytes = send(servfd, buffer + bytes_sent, size - bytes_sent, 0);
        DIE(bytes == -1, "send");
        bytes_sent += bytes;
    }
    // receive response from server
    char *msg_end;
    bytes_received = 0;
    while(1)
    {
        bytes = recv(servfd, buffer + bytes_received, BUFLEN - bytes_received, 0);
        DIE(bytes == -1, "recv");
        if(bytes == 0) break;
        bytes_received += bytes;
        msg_end = strstr(buffer, "\r\n\r\n");
        if(msg_end != NULL)
            break;
    }
    msg_end += sizeof("\r\n\r\n") - 1;
    int content_len = strtol(strstr(buffer, "Content-Length: ") + sizeof("Content-Length: ") - 1, NULL, 10);
    int bytes_remaining = 0;
    bytes_remaining += content_len - (bytes_received - (msg_end - buffer));
    while(bytes_remaining > 0)
    {
        bytes = recv(servfd, buffer + bytes_received, BUFLEN - bytes_received, 0);
        DIE(bytes == -1, "recv");
        if(bytes == 0) break;
        bytes_received += bytes;
        bytes_remaining -= bytes;
    }
    buffer[bytes_received] = '\0';
}
