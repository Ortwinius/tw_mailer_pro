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
      std::cerr << "Unable to accept client_addr connection.\n";
      continue; // Continue accepting other connections
    }

    // get client_addr IP (convert it from network to address format)
    std::string client_addr_ip = inet_ntoa(client_addr.sin_addr);
    pid_t = fork();

    if (pid_t < 0) {
      std::cerr << "Error: Fork failed" << std::endl;
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
  ssize_t bufferSize = ServerConstants::BUFFER_SIZE;
  char *buffer = new char[bufferSize];

  bool validFormat = true;

  while (true) {
    ssize_t totalReceived = recv(consfd, buffer, bufferSize - 1, 0);
    if (totalReceived <= 0) {
      std::cerr << "Receive error or connection closed.\n";
      break;
    }

    buffer[totalReceived] = '\0'; // Null-terminate the received message at

    std::string message(buffer);

    std::istringstream stream(message); // create stream for msg buffer
    std::string command;
    std::getline(stream, command); // get Command (first line)

    std::string contentLengthHeader;
    std::getline(stream, contentLengthHeader);
    int contentLength;

    int headerAndCommandLength = command.length() + 1 + contentLengthHeader.length() + 1; // +1 for each newLine

    // check if contentLengthHeader is correct Format(content-length: <length>),
    // get length
    if (checkContentLengthHeader(contentLengthHeader, contentLength)) {
      if (contentLength + headerAndCommandLength > totalReceived) {
        resize_buffer(buffer, bufferSize, headerAndCommandLength + contentLength + 1);

        ssize_t remaining = bufferSize - totalReceived;
        ssize_t received = recv(consfd, &buffer[totalReceived], remaining, 0);

        totalReceived += received;

        buffer[totalReceived] = '\0';

        // std::cout<<"TotalReceived: "<<totalReceived<<" | Buffer size:
        // "<<bufferSize<<std::endl;
        message = std::string(buffer);
      }
    } 
    else {
      validFormat = false;
    }

    std::cout << "\n\nReceived: " << buffer << "\n";

    if (!validFormat) {
      send_error(consfd, "Message has invalid format"); // Respond with an error message
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
        send_error(consfd, "Message has unknown command"); // Respond with an error message
      }
    } 
    else {
      std::cerr << "User unauthorized" << std::endl;
      send_server_response(consfd, ServerConstants::RESPONSE_UNAUTHORIZED, 13, 0);
      //send(consfd, ServerConstants::RESPONSE_UNAUTHORIZED, 13, 0);
    }
  }
  delete[] buffer; // Free the buffer memory
  close(consfd);   // Ensure the peer socket is closed
}

bool Server::checkContentLengthHeader(std::string &contentLengthHeader, int &contentLength) {
  if (contentLengthHeader.rfind("Content-Length:", 0) == 0) // Starts with "content-length:"
  {
    // get the actual length
    std::string lengthStr = contentLengthHeader.substr(15);
    lengthStr.erase(0, lengthStr.find_first_not_of(" \t")); // Trim leading spaces
    lengthStr.erase(lengthStr.find_last_not_of(" \t") + 1);

    try {
      contentLength = std::stoul(lengthStr); // Convert to size_t
    } catch (const std::invalid_argument &) {
      std::cout << "Invalid content-length value." << std::endl;
      return false;
    } catch (const std::out_of_range &) {
      std::cout << "Content-length value out of range." << std::endl;
      return false;
    }
  } 
  else {
    std::cout << "Invalid format" << std::endl;
    return false;
  }
  return true;
}

void Server::handle_login(int consfd, const std::string &buffer, std::string &authenticatedUser, bool &loggedIn, std::string client_addr_ip) {
  if(blacklist.is_blacklisted(client_addr_ip))
  {
     send_error(consfd, "Blacklisted IP tried to login");
     return;
  }
  
  attempted_logins_cnt++;
  if (attempted_logins_cnt > ServerConstants::MAX_LOGIN_ATTEMPTS) {

    blacklist.add(client_addr_ip);
    attempted_logins_cnt=0;
    std::cout << "Too many failed login attempts, IP " << client_addr_ip << " is now blacklisted." << std::endl;
    send_error(consfd, "Too many login attempts");
    return;
  }

  std::string line;
  std::string username;
  std::string password;

  std::istringstream iss(buffer);

  std::getline(iss, line, '\n'); // skip command and content-length header
  std::getline(iss, line, '\n');

  if (!std::getline(iss, username) || username.empty()) {
    send_error(consfd, "Invalid sender in LOGIN");
    return;
  }

  if (!std::getline(iss, password) || password.empty()) {
    send_error(consfd, "Invalid password in LOGIN");
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
      send(consfd, ServerConstants::RESPONSE_OK, 3, 0);
    }
    else
    {
      std::cerr << "Invalid credentials for " << username << std::endl;
      send_error(consfd, "Invalid credentials in LOGIN");
      loggedIn = false;
      authenticatedUser.clear();
      attempted_logins_cnt++;
    }
  }
  catch(const std::exception &e)
  {
    std::cerr << e.what() << '\n';
  }
  
}

const void Server::send_error(const int consfd, const std::string errorMessage) {
  std::cout << "Send Error to client_addr - " << errorMessage << std::endl;
  send(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
}