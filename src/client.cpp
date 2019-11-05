/**
  @author: Zach Halzel
**/

#include <iostream>
#include <fstream>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <getopt.h>

#define BUF_SIZE 2000

std::string PORT = "2012";
std::string HOST = "127.0.0.1";

void get_options(int argc, char **argv)
{
    extern char *optarg;
    int choice = 0;
    int option_index = 0;

    static struct option long_options[] = {
        {"PORT", optional_argument, NULL, 0},
         {"HOST", optional_argument, NULL, 0},
        {NULL, 0, NULL, 0}};

    while ((choice = getopt_long(argc, argv, "", long_options, &option_index)) != EOF)
    {
        switch (choice)
        {
        case 0:
            if (!strncmp(long_options[option_index].name, "PORT", 4))
            {
                PORT = optarg;
            }
            if (!strncmp(long_options[option_index].name, "HOST", 4))
            {
                HOST = optarg;
            }
            break;
        }
    }
}

void free_exit(struct addrinfo *serv)
{
    freeaddrinfo(serv);
    exit(-1);
}

int receive_from_server(int sock, char *buffer, struct addrinfo *serv)
{
    memset(buffer, 0, BUF_SIZE);
    int bytes_received = 0;
    int total = 0;
    while (!(bytes_received = read(sock, buffer, BUF_SIZE)))
    {
        if (bytes_received == -1)
        {
            std::cerr << "recv error: " << strerror(errno) << std::endl;
            free_exit(serv);
        }
        total += bytes_received;
    }
    return total;
}

int send_all(int sock, char *buf, int *len)
{
    // Function used from http://beej.us/guide/bgnet/html/single/bgnet.html
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;
    while (total < *len)
    {
        n = send(sock, buf + total, bytesleft, 0);
        if (n == -1)
        {
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *len = total;            // return number actually sent here
    return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}

void send_to_server(int sock, std::string img_file)
{
    int length = img_file.length();

    int bytes_sent = send_all(sock, (char *)img_file.c_str(), &length);
    if (bytes_sent)
    {
        perror("sending to server");
    }
}

int get_sock(std::string host, std::string port, struct addrinfo *serv)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints)); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;      // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets

    int status = getaddrinfo(host.c_str(), port.c_str(), &hints, &serv);
    if (status)
    {
        perror("getaddrinfo");
        free_exit(serv);
        return -1;
    }

    int sock = socket(serv->ai_family, serv->ai_socktype, serv->ai_protocol);
    if (sock < 0)
    {
        perror("Socket creation error");
        free_exit(serv);
        return -1;
    }

    if (connect(sock, serv->ai_addr, serv->ai_addrlen) < 0)
    {
        perror("Connection Failed");
        free_exit(serv);
        return -1;
    }
    return sock;
}

int main(int argc, char **argv)
{
    get_options(argc, argv);

    char buffer[BUF_SIZE] = {0};

    struct addrinfo *serv = 0;

    int sock = get_sock(HOST, PORT, serv);

    // receive connection status
    receive_from_server(sock, buffer, serv);
    std::cout << "Connection status: " << buffer << std::endl;

    if (strncmp("Connection successful", buffer, 21)) { // Connection refused
        close(sock);
        freeaddrinfo(serv);
        return 0;
    }

    std::string file_name = "";

    std::cout << "type a file name" << std::endl;
    std::cin >> file_name;

    // get the file
    std::ifstream file(file_name, std::ifstream::in);

    // read the file into a string
    std::string file_string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    send_to_server(sock, file_string);

    // receive return code
    receive_from_server(sock, buffer, serv);

    std::string output = strdup(buffer);

    std::cout << "received from server: \n"
              << output << "\n"
              << std::endl;

    close(sock);
    freeaddrinfo(serv);

    return 0;
}
