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
    logged_in = false;
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
    std::string input;
    bool input_valid = false;

    while (true) {
        std::cout << "> ";  
        std::getline(std::cin, input);

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

        if(input_valid && logged_in)
        {
            handle_response();
            input_valid = false; //reset validation flag
        }
    }
}

// Method to send a simple command
void Client::send_command(const std::string &command) {
    if (send(socket_fd, command.c_str(), command.length(), 0) == -1) {
        std::cerr << "Sending failed" << std::endl;
    }
}

// Method to merge a multiline command (e.g., for SEND)
void Client::merge_multiline_command(const std::string &initial_command) {
    std::string buffer = initial_command + "\n";
    std::string input;

    std::cout << "Enter message content. End with a single '.' on a line by itself:\n";

    while (true) {
        std::getline(std::cin, input);
        if (input == ".") {
            buffer += ".\n";
            break;
        }
        buffer += input + "\n";
    }

    send_command(buffer);
}

void Client::handle_login()
{
    if (logged_in)
    {
        std::cerr << "Error: Already logged in" << std::endl;
        return;
    }

    std::string password;

    std::cout << "Username: ";
    std::cin >> username;
    std::cout << "Password: ";
    std::cin >> password;

    std::string command = "LOGIN\n" + username + "\n" + password;
    send_command(command);

    // TODO: logic if server returned OK ? -> set logged_in to true
}                                                    

// Method to handle the SEND command
void Client::handle_send() 
{
    // HARDCODED TEST
    std::string messageBody="Valentin\nOrtwin\nMail1\nHallo mein Name ist Valentin\nLG\n.\n";
    std::string messageToSend = "SEND\nContent-Length:" + std::to_string(messageBody.length()) + "\n"+messageBody;

    send_command(messageToSend);
    return;
    // TEST ENDE

    if (!validate_action()) return;

    std::string sender, receiver, subject;
    
    // to be deleted
    std::cout << "Sender: ";
    std::getline(std::cin, sender);

    std::cout << "Receiver: ";
    std::getline(std::cin, receiver);

    std::cout << "Subject: ";
    std::getline(std::cin, subject);

    std::string initial_command = "SEND\n" + sender + "\n" + receiver + "\n" + subject;
    merge_multiline_command(initial_command);
}

// Method to handle the LIST command
void Client::handle_list() {
    if (!validate_action()) return;

    std::string username;

    // to be deleted
    std::cout << "Username: ";
    std::getline(std::cin, username);

    std::string command = "LIST\n" + username + "\n";
    send_command(command);
}

// Method to handle the READ command
void Client::handle_read() {
    if (!validate_action()) return;

    std::string username, msg_number;

    // to be deleted
    std::cout << "Username: ";
    std::getline(std::cin, username);

    std::cout << "Message number: ";
    std::getline(std::cin, msg_number);

    std::string command = "READ\n" + username + "\n" + msg_number + "\n";
    send_command(command);
}

// Method to handle the DELETE command
void Client::handle_delete() {
    if (!validate_action()) return;

    std::string username, msg_number;

    // to be deleted
    std::cout << "Username: ";
    std::getline(std::cin, username);

    std::cout << "Message number: ";
    std::getline(std::cin, msg_number);

    std::string command = "DEL\n" + username + "\n" + msg_number + "\n";
    send_command(command);
}

// Method to handle the QUIT command
void Client::handle_quit() {
    send_command("QUIT\n");
}

bool Client::validate_action()
{
    if (!logged_in)
    {
        std::cerr << "You are not logged in yet. You cant perform this action" << std::endl;
        return false;
    }
    return true;
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

