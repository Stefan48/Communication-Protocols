#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <json-c/json.h>
#include "requests.h"
using namespace std;

int main()
{
    int servfd = socket(PF_INET, SOCK_STREAM, 0);
    DIE(servfd == -1, "socket");
    string host = "ec2-3-8-116-10.eu-west-2.compute.amazonaws.com";
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    inet_aton(get_ip(host.c_str()).c_str(), &serv_addr.sin_addr); // 3.8.116.10
    int ret = connect(servfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    DIE(ret == -1,  "connect");
    
    bool logged_in = false;
    bool library_access = false;
    
    string cmd;
    string username;
    string password;
    string id;
    string title;
    string author;
    string genre;
    string publisher;
    string page_count;
    char *err;
    char *book;
    
    string message;
    string data;
    string cookie;
    string token;
    char buffer[BUFLEN];
    
    struct json_object *parsed_json;
    struct json_object *parsed_error;
    struct json_object *parsed_token;
    struct json_object *parsed_id;
    struct json_object *parsed_title;
    struct json_object *parsed_author;
    struct json_object *parsed_genre;
    struct json_object *parsed_publisher;
    struct json_object *parsed_page_count;
    
    while(1)
    {
        getline(cin, cmd);
        if(cmd == "exit") goto stop_client;
        if(cmd == "register")
        {
            cout << "username=";
            getline(cin, username);
            cout << "password=";
            getline(cin, password);
            data = "{\"username\":\"" + username + "\", \"password\":\"" + password + "\"}";
            message = compute_post_request(host, "/api/v1/tema/auth/register", "application/json", data, "\0", "\0");
            send_and_receive(servfd, serv_addr, message, buffer);
            err = strstr(buffer, "{\"error");
            if(err != NULL)
            {
                parsed_json = json_tokener_parse(err);
                json_object_object_get_ex(parsed_json, "error", &parsed_error);
                cout << json_object_get_string(parsed_error) << "\n";
                json_object_put(parsed_json);
            }
            else
            {
                cout << "Registration complete.\n";
            }
            continue;
        }
        if(cmd == "login")
        {
            if(logged_in)
                cout << "Already logged in.\n";
            else
            {
                cout << "username=";
                getline(cin, username);
                cout << "password=";
                getline(cin, password);
                data = "{\"username\":\"" + username + "\", \"password\":\"" + password + "\"}";
                message = compute_post_request(host, "/api/v1/tema/auth/login", "application/json", data, "\0", "\0");
                send_and_receive(servfd, serv_addr, message, buffer);
                err = strstr(buffer, "{\"error");
                if(err != NULL)
                {
                    parsed_json = json_tokener_parse(err);
                    json_object_object_get_ex(parsed_json, "error", &parsed_error);
                    cout << json_object_get_string(parsed_error) << "\n";
                    json_object_put(parsed_json);
                }
                else
                {
                    logged_in = true;
                    cout << "Successfully logged in.\n";
                    cookie = strtok(strstr(buffer, "connect.sid="), ";");
                }
            }
            continue;
        }
        if(cmd == "enter_library")
        {
            if(!logged_in)
            {
                cout << "Currently not logged in.\n";
            }
            else if(library_access)
            {
                cout << "Access to library has already been granted.\n";
            }
            else
            {
                message = compute_get_request(host, "/api/v1/tema/library/access", "\0", cookie, "\0");
                send_and_receive(servfd, serv_addr, message, buffer);
                parsed_json = json_tokener_parse(strstr(buffer, "{\"token\""));
                json_object_object_get_ex(parsed_json, "token", &parsed_token);
                token = json_object_get_string(parsed_token);
                json_object_put(parsed_json);
                library_access = true;
                cout << "Access granted.\n";
            }
            continue;
        }
        if(cmd == "get_books")
        {
            if(!logged_in)
            {
                cout << "Currently not logged in.\n";
            }
            else if(!library_access)
            {
                cout << "You do not have permission to access the library.\n";
            }
            else
            {
                message = compute_get_request(host, "/api/v1/tema/library/books", "\0", "\0", token);
                send_and_receive(servfd, serv_addr, message, buffer);
                book = strstr(buffer, "{\"id");
                if(book == NULL)
                {
                    cout << "No books found.\n";
                }
                else
                {
                    while(book != NULL)
                    {
                        parsed_json = json_tokener_parse(book);
                        json_object_object_get_ex(parsed_json, "id", &parsed_id);
                        json_object_object_get_ex(parsed_json, "title", &parsed_title);
                        cout << "Id: " << json_object_get_string(parsed_id) << ", title: " << json_object_get_string(parsed_title) << "\n";
                        book = strstr(book + 1, "{\"id");
                        json_object_put(parsed_json);
                    }
                }
            }
            continue;
        }
        if(cmd == "get_book")
        {
            if(!logged_in)
            {
                cout << "Currently not logged in.\n";
            }
            else if(!library_access)
            {
                cout << "You do not have permission to access the library.\n";
            }
            else
            {
                cout << "id=";
                getline(cin, id);
                bool valid = true;
                for(int i = 0; id[i] != '\0'; ++i)
                    if(!isdigit(id[i]))
                    {
                        valid = false;
                        break;
                    }
                if(!valid)
                {
                    cout << "Invalid id.\n";
                }
                else
                {
                    message = compute_get_request(host, "/api/v1/tema/library/books/" + id, "\0", "\0", token);
                    send_and_receive(servfd, serv_addr, message, buffer);
                    err = strstr(buffer, "{\"error");
                    if(err != NULL)
                    {
                        parsed_json = json_tokener_parse(err);
                        json_object_object_get_ex(parsed_json, "error", &parsed_error);
                        cout << json_object_get_string(parsed_error) << "\n";
                        json_object_put(parsed_json);
                    }
                    else
                    {
                        parsed_json = json_tokener_parse(strstr(buffer, "{\""));
                        json_object_object_get_ex(parsed_json, "title", &parsed_title);
                        json_object_object_get_ex(parsed_json, "author", &parsed_author);
                        json_object_object_get_ex(parsed_json, "genre", &parsed_genre);
                        json_object_object_get_ex(parsed_json, "publisher", &parsed_publisher);
                        json_object_object_get_ex(parsed_json, "page_count", &parsed_page_count);
                        cout << "Title: " << json_object_get_string(parsed_title) <<
                                "\nAuthor: " << json_object_get_string(parsed_author) <<
                                "\nGenre: " << json_object_get_string(parsed_genre) <<
                                "\nPublisher: " << json_object_get_string(parsed_publisher) <<
                                "\nPage count: " << json_object_get_string(parsed_page_count) << "\n";
                        json_object_put(parsed_json);
                    }
                }
            }
            continue;
        }
        if(cmd == "add_book")
        {
            if(!logged_in)
            {
                cout << "Currently not logged in.\n";
            }
            else if(!library_access)
            {
                cout << "You do not have permission to access the library.\n";
            }
            else
            {
                cout << "title=";
                getline(cin, title);
                cout << "author=";
                getline(cin, author);
                cout << "genre=";
                getline(cin, genre);
                cout << "publisher=";
                getline(cin, publisher);
                cout << "page_count=";
                getline(cin, page_count);
                bool valid = true;
                for(int i = 0; page_count[i] != '\0'; ++i)
                    if(!isdigit(page_count[i]))
                    {
                        valid = false;
                        break;
                    }
                if(!valid)
                {
                    cout << "Invalid page count.\n";
                }
                else
                {
                    data = "{\"title\":\"" + title + "\", \"author\":\"" + author + "\", \"genre\":\"" + genre + 
                    "\", \"publisher\":\"" + publisher + "\", \"page_count\":\"" + page_count + "\"}";
                    message = compute_post_request(host, "/api/v1/tema/library/books", "application/json", data, "\0", token);
                    send_and_receive(servfd, serv_addr, message, buffer);
                    cout << "Successfully added book.\n";
                }
            }
            continue;
        }
        if(cmd == "delete_book")
        {
            if(!logged_in)
            {
                cout << "Currently not logged in.\n";
            }
            else if(!library_access)
            {
                cout << "You do not have permission to access the library.\n";
            }
            else
            {
                cout << "id=";
                getline(cin, id);
                bool valid = true;
                for(int i = 0; id[i] != '\0'; ++i)
                    if(!isdigit(id[i]))
                    {
                        valid = false;
                        break;
                    }
                if(!valid)
                {
                    cout << "Invalid id.\n";
                }
                else
                {
                    message = compute_delete_request(host, "/api/v1/tema/library/books/" + id, token);
                    send_and_receive(servfd, serv_addr, message, buffer);
                    err = strstr(buffer, "{\"error");
                    if(err != NULL)
                    {
                        parsed_json = json_tokener_parse(err);
                        json_object_object_get_ex(parsed_json, "error", &parsed_error);
                        cout << json_object_get_string(parsed_error) << "\n";
                        json_object_put(parsed_json);
                    }
                    else
                    {
                        cout << "Book deleted.\n";
                    }
                }
            }
            continue;
        }
        if(cmd == "logout")
        {
            if(!logged_in)
            {
                cout << "Currently not logged in.\n";
            }
            else
            {
                logged_in = false;
                library_access = false;
                cout << "Logged out.\n";
            }
            continue;
        }
        cout << "Invalid command.\n";
    }
    
stop_client:
    shutdown(servfd, 2);
    close(servfd);
    return 0;
}
