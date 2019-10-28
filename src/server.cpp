#include <iostream>
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

#define port "8080"

int main (int argc, char *argv[]) { 
    std::cout << "Hello World" << std::endl;
    
    struct addrinfo hints, *result, *pointer;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET; // IPv4 (other option is IPv6) 
    hints.ai_socktype = SOCK_STREAM; // TCP (other optionis UDP)
    hints.ai_flags = IPPROTO_TCP; //
    
    int add = getaddrinfo(NULL, port, &hints, &result); // If result is populated, then the function returns 0
    if (add != 0){
        std::cout << port << std::endl;
        std::cout << add << std::endl;
        std::cout << result->ai_addr << std::endl;
        std::cerr << "getAddrInfo failed" << " " << strerror(errno) << std::endl;
        return -1;
    }

    // Creating a socket
    int sockFD = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sockFD == -1){
        std::cout << "Failure creating socket file descriptor" << std::endl;
        return -1;
    }

    // Binding address to the created socket
    int bindFD = bind(sockFD, result->ai_addr, result->ai_addrlen);
    if (bindFD == -1){
        std::cout << "Failure binding socket address and port number" << std::endl;
        return -1;
    }

 }