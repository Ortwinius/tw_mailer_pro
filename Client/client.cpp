#include "client.h"
#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// Constructor to initialize IP, port, and set up the socket
Client::Client(const std::string& ip, int port) 
: ip_address(ip)
, port(port)
, socket_fd(-1) 
{
    username = "";
    init_socket();
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
        std::cerr << "Socket creation failed." << std::endl;
        exit(EXIT_FAILURE);
    }
}

// Method to connect to the server
void Client::connect_to_server() {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_aton(ip_address.c_str(), &server_addr.sin_addr) == 0) {
        std::cerr << "Invalid IP address" << std::endl;
        exit(EXIT_FAILURE);
    }

    if (connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Connection failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Connected to server " << ip_address << " on port " << port << std::endl;
}

// Method to handle user input and commands
void Client::handle_user_input() {
    //std::unordered_map<std::string, void*> functionMap;

    std::string input;
    bool input_valid = false;

    while (true) {
        getUserInput(">", input);

        if (input == "QUIT") {
            input_valid = true;
            handle_quit();
            break;
        } else if(input == "LOGIN"){
            input_valid = true;
            handle_login();
        }
        else if (input == "SEND") {
            input_valid = true;
            handle_send();
        } else if (input == "LIST") {
            input_valid = true;
            handle_list();
        } else if (input == "READ") {
            input_valid = true;
            handle_read();
        } else if (input == "DEL") {            
            input_valid = true;
            handle_delete();
        } else {
            std::cerr << "Input is not valid: unknown command." << std::endl;
        }

        if(input_valid)
        {
            handle_response();
            input_valid = false; //reset validation flag
        }
    }
}

// Method to send the final command
void Client::send_command(const std::string &command) {
    if (send(socket_fd, command.c_str(), command.length(), 0) == -1) {
        std::cerr << "Sending failed" << std::endl;
    }
}
// Method to handle responses from the server
void Client::handle_response() {
    char buffer[1024];
    ssize_t received = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);

    if (received > 0) {
        buffer[received] = '\0';
        std::cout << "Server: " << buffer << std::endl;
    } else if (received == 0) {
        std::cout << "Server closed the connection\n";
    } else {
        std::cerr << "Receive failed" << std::endl;
    }
}

void Client::handle_login() {
    std::string inputUsername, inputPassword;
    getUserInput("Username: ", inputUsername);
    getHiddenUserInput("Password: ", inputPassword);

    CommandBuilder builder;
    builder.add_parameter(inputUsername);
    builder.add_parameter(inputPassword);

    username = inputUsername; // might be wrong - if so, in the next login-attempt it will be reset

    std::string cmd = builder.build_final_cmd("LOGIN");
    send_command(cmd);
}                                                    

// Method to handle the SEND command
void Client::handle_send() {
    std::string sender, receiver, subject;

    getUserInput("Receiver: ", receiver);
    getUserInput("Subject: ", subject);

    CommandBuilder builder;
    builder.add_parameter(username); // if user is not logged in -> username is empty
    builder.add_parameter(receiver);
    builder.add_parameter(subject);
    builder.add_msg_content();

    std::string command = builder.build_final_cmd("SEND");
    send_command(command);
}

// Method to handle the LIST command
void Client::handle_list() {
    CommandBuilder builder;
    builder.add_parameter(username);

    std::string cmd = builder.build_final_cmd("LIST");
    send_command(cmd);
}

// Method to handle the READ command
void Client::handle_read() {
    std::string msg_number;
    getUserInput("MsgNr: ", msg_number);

    CommandBuilder builder;
    builder.add_parameter(username); 
    builder.add_parameter(msg_number);

    std::string cmd = builder.build_final_cmd("READ");
    send_command(cmd);
}

// Method to handle the DELETE command
void Client::handle_delete() {
    std::string msg_number;
    getUserInput("Message number: ", msg_number);

    CommandBuilder builder;
    builder.add_parameter(username); 
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




