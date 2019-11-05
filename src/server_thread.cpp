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

#define BUF_SIZE 2000 // TODO: add support for larger sized QR codes, or mention size limit

// default variables
std::string PORT = "2012";
int RATE_MSGS = 3;
int RATE_TIME = 60;
int MAX_USERS = 3;
int TIME_OUT = 80;

std::atomic_int num_users(0);

void receive_from_client(int sock, char *buffer)
{
    memset(buffer, 0, BUF_SIZE);
    int bytes_recvd;
    while (!(bytes_recvd = read(sock, buffer, BUF_SIZE)))
    {
        if (bytes_recvd == -1)
        {
            close(sock);
            std::cerr << "child process: invalid recv" << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    errno = saved_errno;
}

struct args
{
    int socket;
    int sockFD;
};

void *thread_func(void *vargs)
{
    int return_code = 0;
    std::string output;

    int socket = ((struct args *)vargs)->socket;
    int sockFD = ((struct args *)vargs)->sockFD;

    std::cout << "socket" << socket << std::endl;
    // if (num_users >= MAX_USERS)
    // {
    //     return_code = 1;
    //     const char *return_code_string = std::to_string(return_code).c_str();

    //     std::string error = "return code: " + std::to_string(return_code) + "\n Too many users connected, try again later :) ";

    //     // send the return code string and URL
    //     if (send(socket, error.c_str(), strlen(error.c_str()), 0) == -1)
    //     {
    //         {
    //             std::cerr << "Error sending to client" << std::endl;
    //         }
    //     }
    //     close(socket); // TODO is this good?
    //     return NULL;
    // }

    // num_users += 1;

    output = "Connection successful";
    std::cout << "got here" << std::endl;
    if (send(socket, output.c_str(), strlen(output.c_str()), 0) == -1)
    {
        {
            std::cerr << "Error sending to client" << std::endl;
        }
    }

    // TODO test a broken image

    char buffer[SO_SNDBUF];

    std::string child_id_str = std::to_string(getpid());
    std::cout << "Forked child, id: " << child_id_str << std::endl;
    close(sockFD);
    receive_from_client(socket, buffer);

    // save buffer as a file
    std::string file_name = "qr/" + child_id_str + ".png";
    std::ofstream outfile(file_name, std::ofstream::binary);
    outfile.write((char *)&buffer, sizeof(buffer));
    outfile.close();

    // process the QR code
    std::string qr_command = "cd qr; java -cp javase.jar:core.jar com.google.zxing.client.j2se.CommandLineRunner --dump_results " + child_id_str + ".png";
    std::cout << "\n***\n"
              << "Running QR Processing\n"
              << std::endl;
    system(qr_command.c_str());
    std::cout << "\n***\n"
              << std::endl;

    // open the qr file results
    std::string text_file = "qr/" + child_id_str + ".txt";
    std::ifstream file(text_file, std::ifstream::in);

    // read the file into a string
    std::string url_string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // send results to client

    const char *return_code_string = std::to_string(return_code).c_str();

    output = "return code: " + std::to_string(return_code) + "\n URL: " + url_string;

    // send the return code string and URL
    if (send(socket, output.c_str(), strlen(output.c_str()), 0) == -1)
    {
        {
            std::cerr << "Error sending the URL" << std::endl;
        }
    }
    close(socket); // TODO is this good?
    num_users -= 1;
    return NULL;
}

void get_options(int argc, char **argv)
{
    extern char *optarg;
    int choice = 0;
    int option_index = 0;

    static struct option long_options[] = {
        {"PORT", optional_argument, NULL, 0},
        {"RATE_MSGS", optional_argument, NULL, 0},
        {"RATE_TIME", optional_argument, NULL, 0},
        {"MAX_USERS", optional_argument, NULL, 0},
        {"TIME_OUT", optional_argument, NULL, 0},
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
            if (!strncmp(long_options[option_index].name, "RATE_MSGS", 9))
            {
                RATE_MSGS = atoi(optarg);
            }
            if (!strncmp(long_options[option_index].name, "RATE_TIME", 9))
            {
                RATE_TIME = atoi(optarg);
            }
            if (!strncmp(long_options[option_index].name, "MAX_USERS", 9))
            {
                MAX_USERS = atoi(optarg);
            }
            if (!strncmp(long_options[option_index].name, "TIME_OUT", 8))
            {
                TIME_OUT = atoi(optarg);
            }
            break;
        }
    }
}

void server_routine(int sockFD)
{

    if (listen(sockFD, 10) < 0)
        { // listen queue is 10
            std::cerr << "listen failed" << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }
        
    struct sockaddr_storage address;
    socklen_t addrlen;
    struct sigaction sig_action;
    int new_socket;

    pthread_t thread;

    if ((new_socket = accept(sockFD, (struct sockaddr *)&address, &addrlen)) < 0)
    {
        std::cerr << "accept() " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    struct args *args = (struct args *)malloc(sizeof(struct args)); // TODO un-malloc
    
    args->socket = new_socket;
    args->sockFD = sockFD;

    std::cout << "Client request received" << std::endl;

    if (pthread_create(&thread, NULL, thread_func, (void *)args))
    {
        fprintf(stderr, "Error creating thread\n");
    }

    // clean up all processes
    sig_action.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sig_action.sa_mask);
    sig_action.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sig_action, NULL) == -1)
    {
        std::cerr << "sigaction" << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
    close(new_socket);
}

int main(int argc, char *argv[])
{
    get_options(argc, argv);

    std::cout << "Activating server, options are:" << std::endl;
    std::cout << "PORT: " << PORT << ", RATE_MSGS: " << RATE_MSGS << ", RATE_TIME: " << RATE_TIME << ", MAX_USERS: " << MAX_USERS << ", TIME_OUT: " << TIME_OUT << std::endl;

    struct addrinfo hints, *result, *pointer;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;       // IPv4 (other option is IPv6)
    hints.ai_socktype = SOCK_STREAM; // TCP (other optionis UDP)
    hints.ai_flags = IPPROTO_TCP;

    int add = getaddrinfo(NULL, PORT.c_str(), &hints, &result); // If result is populated, then the function returns 0

    if (add)
    {
        if (add == EAI_SYSTEM)
        {
            fprintf(stderr, "EAI_SYSTEM error in getaddrinfo", strerror(errno));
        }
        else
        {
            fprintf(stderr, "other error in getaddrinfo", gai_strerror(add));
        }
        exit(EXIT_FAILURE);
    }

    // Creating a socket
    int sockFD = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

    if (sockFD == -1)
    {
        std::cout << "Failure creating socket file descriptor" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Binding address to the created socket
    int bindFD = bind(sockFD, result->ai_addr, result->ai_addrlen);

    if (bindFD == -1)
    {
        std::cout << "Failure binding socket address and port number" << std::endl;
        exit(EXIT_FAILURE);
    }
    
    while (true)
    {
        

        server_routine(sockFD);
    }

    close(sockFD);
    return 0;
}
