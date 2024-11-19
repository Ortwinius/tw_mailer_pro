#include "client.h"
#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sstream>
#include "../utils/constants.h"

// Constructor to initialize IP, port, and set up the socket
Client::Client(const std::string& ip, int port) 
: ip_address(ip)
, port(port)
, socket_fd(-1) 
{
    init_socket();
    init_function_routing();
}

// Destructor to close the socket if open
Client::~Client() {
    if (socket_fd != -1) {
        close(socket_fd);
    }
}

// Method to start the client
void Client::start() {
    connect_to_server();
    handle_user_input();
}

// Method to initialize the socket
void Client::init_socket() {
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        std::cout << "Socket creation failed." << std::endl;
        exit(EXIT_FAILURE);
    }
}
void Client::init_function_routing() {
    command_map = {
        {"QUIT", [&]() { handle_quit(); }},
        {"LOGIN", [&]() { handle_login(); }},
        {"SEND", [&]() { handle_send(); }},
        {"LIST", [&]() { handle_list(); }},
        {"READ", [&]() { handle_read(); }},
        {"DEL", [&]() { handle_delete(); }}
    };
}

// Method to connect to the server
void Client::connect_to_server() {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_aton(ip_address.c_str(), &server_addr.sin_addr) == 0) {
        std::cout << "Invalid IP address" << std::endl;
        exit(EXIT_FAILURE);
    }

    if (connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cout << "Connection failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Connected to server " << ip_address << " on port " << port << std::endl;
}

// Method to handle user input and commands
void Client::handle_user_input() {
    std::string input;

    while (true) {
        get_user_input(">", input);

        if(command_map.find(input) != command_map.end())
        {
            command_map[input](); // call corresponding cmd function
        }
        else 
        {
            std::cout << "Input is not valid: unknown command. Try again.\n";
            continue;
        }

        // if cmd was valid and executed -> handle response
        handle_response();
    }
}

// Method to send the final command
void Client::send_command(const std::string &command) {
    if (send(socket_fd, command.c_str(), command.length(), 0) == -1) {
        std::cout << "Sending failed" << std::endl;
    }
}
// Method to handle responses from the server
void Client::handle_response() {  
    std::string response; // To hold the full response
    ssize_t init_size = GenericConstants::STD_BUFFER_SIZE;
    char *buffer=new char[init_size]; // temporary buffer for chunks to be read in incrementally
    
    ssize_t total_received=recv(socket_fd, buffer, init_size-1, 0);
    if (total_received <= 0) {
      std::cout << "Receive error or connection closed.\n";
      return;
    }

    buffer[total_received]='\0';

    std::istringstream stream(buffer);

    int content_length;
    std::string content_length_header;
    std::getline(stream, content_length_header);

    if(check_content_length_header(content_length_header, content_length))
    {
        int message_length=content_length_header.size()+content_length+1;
        if(message_length>total_received)
        {
            resize_buffer(buffer, init_size, message_length+1);

            ssize_t remaining = message_length-total_received;
            ssize_t received=recv(socket_fd, &buffer[total_received], remaining, 0);

            total_received+=received;
            buffer[total_received]='\0';
        }

        // Skip the first line (content-length header) and print the rest
        const char* body_start = buffer + content_length_header.size() + 1;
        std::cout<<"\nServer Response:"<<std::endl;
        std::cout << body_start << std::endl;
    }
    else
    {
        std::cout<<"Received response with invalid format"<<std::endl;
    }


    delete[] buffer;
}

void Client::handle_login() {
    std::string inputUsername, inputPassword;
    get_user_input("Username: ", inputUsername);
    get_hidden_user_input("Password: ", inputPassword);

    CommandBuilder builder;
    builder.add_parameter(inputUsername);
    builder.add_parameter(inputPassword);

    std::string cmd = builder.build_final_cmd("LOGIN");
    send_command(cmd);
}                                                    

// Method to handle the SEND command
void Client::handle_send() {
    std::string sender, receiver, subject;

    get_user_input("Receiver: ", receiver);
    get_user_input("Subject: ", subject);

    CommandBuilder builder;
    builder.add_parameter(receiver);
    builder.add_parameter(subject);
    builder.add_msg_content();

    std::string cmd = builder.build_final_cmd("SEND");
    send_command(cmd);
}

// Method to handle the LIST command
void Client::handle_list() {
    CommandBuilder builder;

    std::string cmd = builder.build_final_cmd("LIST");
    send_command(cmd);
}

// Method to handle the READ command
void Client::handle_read() {
    std::string msg_number;
    get_user_input("MsgNr: ", msg_number);

    CommandBuilder builder;
    builder.add_parameter(msg_number);

    std::string cmd = builder.build_final_cmd("READ");
    send_command(cmd);
}

// Method to handle the DELETE command
void Client::handle_delete() {
    std::string msg_number;
    get_user_input("Message number: ", msg_number);

    CommandBuilder builder;
    builder.add_parameter(msg_number);

    std::string cmd = builder.build_final_cmd("DEL");
    send_command(cmd);
}

// Method to handle the QUIT command
void Client::handle_quit() {
    CommandBuilder builder;
    std::string cmd = builder.build_final_cmd("QUIT");
    send_command(cmd);
}




