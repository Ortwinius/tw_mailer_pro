#include "server.h"
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <ldap.h>
#include <lber.h>
#include "../utils/helpers.h"
#include "../utils/constants.h"

Server::Server(int port, const fs::path &mailDirectory) 
: port(port)
, mail_manager(mailDirectory)
, blacklist()
{ 
  attempted_logins_cnt = 0;
  init_socket(); 
}

Server::~Server() 
{ 
  close(socket_fd); 
}

void Server::run() 
{ 
  listen_for_connections(); 
}

void Server::init_socket() 
{
  // AF_INET (IPv4), SOCK_STREAM (typically TCP), 0 (default protocol)
  socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    throw std::runtime_error("Error initializing the server socket: " + std::to_string(errno));
  }

  std::cout << "Socket was created with id " << socket_fd << "\n";

  // set up server address struct to bind socket to a specific address
  struct sockaddr_in serveraddr;
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = INADDR_ANY; // Accept connections from any interface
  serveraddr.sin_port = htons(port);

  // set socket options (SO_REUSEADDRE - reusing local address and same port
  // even if in TIME_WAIT)
  int enable = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1) {
    throw std::runtime_error("Unable to set socket options due to: " + std::to_string(errno));
  }

  // binds the socket with a specific address
  if (bind(socket_fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
    throw std::runtime_error("Binding failed with error: " + std::to_string(errno));
  }

  // socket starts listening for connection requests
  if (listen(socket_fd, ServerConstants::MAX_PENDING_CONNECTIONS) == -1) {
    throw std::runtime_error("Unable to establish connection: " + std::to_string(errno));
  }
}

void Server::listen_for_connections() {
  int pid_t;

  // init semaphore
  sem_t sem;
  sem_init(&sem, 1, 1);

  // Automatically clean up child processes
  // Parent process doesnt check exit status, Kernel reclaims ressources
  signal(SIGCHLD, SIG_IGN);

  struct sockaddr_in client_addr; 
  socklen_t addrlen = sizeof(client_addr);

  while (true) {
    int peersoc = accept(socket_fd, (struct sockaddr *) &client_addr, &addrlen);
    if (peersoc == -1) {
      std::cout << "Unable to accept client_addr connection.\n";
      continue; // Continue accepting other connections
    }

    // get client_addr IP (convert it from network to address format)
    std::string client_addr_ip = inet_ntoa(client_addr.sin_addr);
    pid_t = fork();

    if (pid_t < 0) {
      std::cout << "Error: Fork failed" << std::endl;
      exit(EXIT_FAILURE);
    } 
    else if (pid_t == 0) {
      std::cout << "Accepted connection with file descriptor: " << peersoc << "\n";
      close(socket_fd);
      // enter main cmd loop
      handle_communication(peersoc, &sem, client_addr_ip);
      exit(EXIT_SUCCESS);
    } 
    else {
      std::cout << "Parent process: forked child " << pid_t << " to handle communication with file descriptor  " << peersoc << "\n";
      close(peersoc);
    }
  }
}

void Server::handle_communication(int consfd, sem_t *sem, std::string client_addr_ip) {
  ssize_t buffer_size = ServerConstants::BUFFER_SIZE;
  char *buffer = new char[buffer_size];

  bool valid_format = true;

  while (true) {
    ssize_t total_received = recv(consfd, buffer, buffer_size - 1, 0);
    if (total_received <= 0) {
      std::cout << "Receive error or connection closed.\n";
      break;
    }

    buffer[total_received] = '\0'; // Null-terminate the received message

    std::istringstream stream(buffer); // create stream for msg buffer
    std::string command;
    std::getline(stream, command); // get Command (first line)

    std::string content_length_header;
    std::getline(stream, content_length_header);
    int content_length;

    // check if contentLengthHeader is correct Format(content-length: <length>),
    // get length
    if (check_content_length_header(content_length_header, content_length)) {
      int message_length = command.length() + 1 + content_length_header.length() + 1 + content_length;  // +1 for newLines
      if (message_length > total_received) {
        resize_buffer(buffer, buffer_size, message_length + 1);

        ssize_t remaining = buffer_size - total_received;
        ssize_t received = recv(consfd, &buffer[total_received], remaining, 0);

        total_received += received;

        buffer[total_received] = '\0';
      }
    } 
    else {
      valid_format = false;
    }

    std::cout << "\n\nReceived: " << buffer << "\n";

    if (!valid_format) {
      std::cout << "Message has invalid format" << std::endl;
      send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0); // Respond with an error message
    }

    // QUIT to close conn
    if (command == "QUIT") {
      std::cout << "Closing connection with client_addr [FileDescriptor: " << consfd << "]\n";
      close(consfd);
      break;
    }
    if (command == "LOGIN") {
      std::cout << "Processing LOGIN command" << std::endl;
      handle_login(consfd, buffer, authenticatedUser, loggedIn, client_addr_ip);
    } 
    else if (loggedIn) {
      if (command == "SEND") {
        std::cout << "Processing SEND command" << std::endl;
        mail_manager.handle_send(consfd, buffer, authenticatedUser, sem);
      } 
      else if (command == "LIST") {
        std::cout << "Processing LIST command" << std::endl;
        mail_manager.handle_list(consfd, authenticatedUser, sem);
      } 
      else if (command == "READ") {
        std::cout << "Processing READ command" << std::endl;
        mail_manager.handle_read(consfd, buffer, authenticatedUser, sem);
      } 
      else if (command == "DEL") {
        std::cout << "Processing DEL command" << std::endl;
        mail_manager.handle_delete(consfd, buffer, authenticatedUser, sem);
      } 
      else {
        std::cout << "Message has unknown command" << std::endl;
        send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0); // Respond with an error message
      }
    } 
    else {
      std::cout << "User unauthorized" << std::endl;
      send_server_response(consfd, ServerConstants::RESPONSE_UNAUTHORIZED, 13, 0);
    }
  }
  delete[] buffer; // Free the buffer memory
  close(consfd);   // Ensure the peer socket is closed
}

void Server::handle_login(int consfd, const std::string &buffer, std::string &authenticatedUser, bool &loggedIn, std::string client_addr_ip) {
  std::cout<<"before checking blackist"<<std::endl; //toDelete
  try{
    if(blacklist.is_blacklisted(client_addr_ip))
    {
      std::cout << "Blacklisted IP tried to login" << std::endl;
      send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0); // Respond with an error message
      return;
    }
  }
  catch(const std::exception& e){
    std::cout<<e.what()<<std::endl;
  }
  std::cout<<"after checking blackist"<<std::endl; //toDelete
  
  attempted_logins_cnt++;
  if (attempted_logins_cnt > ServerConstants::MAX_LOGIN_ATTEMPTS) {
  
    blacklist.add(client_addr_ip);
    attempted_logins_cnt=0;
    std::cout << "Too many failed login attempts, IP " << client_addr_ip << " is now blacklisted." << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0); // Respond with an error message
    return;
  }

  std::string line;
  std::string username;
  std::string password;

  std::istringstream iss(buffer);

  std::getline(iss, line, '\n'); // skip command and content-length header
  std::getline(iss, line, '\n');

  if (!std::getline(iss, username) || username.empty()) {
    std::cout << "Invalid Sender in LOGIN" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0); // Respond with an error message
    return;
  }

  if (!std::getline(iss, password) || password.empty()) {
    std::cout << "Invalid Password in LOGIN" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0); // Respond with an error message
    return;
  }
  
  // initialize ldap client obj and then try authenticate via SASL bind
  std::string host_url = ServerConstants::HOST_URL;
  LDAP_Module ldap_client = LDAP_Module(host_url);

  // try catch block to catch any LDAP related errors
  try
  {
    if(ldap_client.authenticate(username, password))
    {
      loggedIn = true;
      authenticatedUser = username;
      send_server_response(consfd, ServerConstants::RESPONSE_OK, 3, 0);
    }
    else
    {
      std::cout << "Invalid credentials for " << username << "in LOGIN" << std::endl;
      send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
      loggedIn = false;
      authenticatedUser.clear();
      attempted_logins_cnt++;
    }
  }
  catch(const std::exception &e)
  {
    std::cout << e.what() << '\n';
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
  }
  
}