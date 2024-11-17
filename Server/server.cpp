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
  if (listen(socket_fd, 6) == -1) {
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

  struct sockaddr client;
  socklen_t addrlen = sizeof(client);

  while (true) {
    int peersoc = accept(socket_fd, &client, &addrlen);
    if (peersoc == -1) {
      std::cerr << "Unable to accept client connection.\n";
      continue; // Continue accepting other connections
    }

    pid_t = fork();
    if (pid_t < 0) {
      std::cerr << "Error: Fork failed" << std::endl;
      exit(EXIT_FAILURE);
    } 
    else if (pid_t == 0) {
      std::cout << "Accepted connection with file descriptor: " << peersoc << "\n";
      close(socket_fd);
      handle_communication(peersoc, &sem, "127.0.0.1");
      exit(EXIT_SUCCESS);
    } 
    else {
      std::cout << "Parent process: forked child " << pid_t << " to handle communication with file descriptor  " << peersoc << "\n";
      close(peersoc);
    }
  }
}

void Server::handle_communication(int consfd, sem_t *sem, std::string client_ip) {
  ssize_t bufferSize = 64;
  char *buffer = new char[bufferSize];

  bool validFormat = true;

  while (true) {
    ssize_t totalReceived = recv(consfd, buffer, bufferSize - 1, 0);
    if (totalReceived <= 0) {
      std::cerr << "Receive error or connection closed.\n";
      break;
    }

    buffer[totalReceived] = '\0'; // Null-terminate the received message at
                                  // currentSize (counted by recv)

    std::string message(buffer);

    std::istringstream stream(message); // create stream for msg buffer
    std::string command;
    std::getline(stream, command); // get Command (first line)

    std::string contentLengthHeader;
    std::getline(stream, contentLengthHeader);
    int contentLength;

    int headerAndCommandLength = command.length()+ 1 + contentLengthHeader.length()+ 1; // +1 for each newLine

    // check if contentLengthHeader is correct Format(content-length: <length>),
    // get length
    if (checkContentLengthHeader(contentLengthHeader, contentLength)) {
      if (contentLength + headerAndCommandLength > totalReceived) {
        resizeBuffer(buffer, bufferSize, headerAndCommandLength + contentLength + 1);

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
      std::cout << "Closing connection with client [FileDescriptor: " << consfd << "]\n";
      close(consfd);
      break;
    }
    if (command == "LOGIN") {
      std::cout << "Processing LOGIN command" << std::endl;
      handle_login(consfd, buffer, authenticatedUser, loggedIn, client_ip);
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
      std::cout << "User unauthorized" << std::endl;
      send(consfd, "Unauthorized\n", 13, 0);
    }
  }
  delete[] buffer; // Free the buffer memory
  close(consfd);   // Ensure the peer socket is closed
}

void Server::resizeBuffer(char *&buffer, ssize_t &currentSize, ssize_t newCapacity) {
  if (newCapacity <= currentSize) {
    return; // No resizing needed if the new size is smaller or equal
  }

  // Allocate new memory
  char *newBuffer = new char[newCapacity];

  // Copy old data into the new buffer
  std::memcpy(newBuffer, buffer, currentSize);

  // Free the old buffer
  delete[] buffer;

  // Update the buffer pointer and size
  buffer = newBuffer;
  currentSize = newCapacity;
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

void Server::handle_login(int consfd, const std::string &buffer, std::string &authenticatedUser, bool &loggedIn, std::string client_ip) {
  if(blacklist.is_blacklisted(client_ip))
  {
     send_error(consfd, "Blacklisted IP tried to login");
     return;
  }
  
  attempted_logins_cnt++;
  if (attempted_logins_cnt > 3) {

    blacklist.add(client_ip);
    attempted_logins_cnt=0;
    std::cout << "Too many failed login attempts, IP " << client_ip << " is now blacklisted." << std::endl;
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

  LDAP *ldap_obj = nullptr;
  const char *host = "ldap.technikum-wien.at";
  int ldap_port = 389; 

  std::string ldap_url = "ldap://" + std::string(host) + ":" + std::to_string(ldap_port);

  int result = ldap_initialize(&ldap_obj, ldap_url.c_str());

  if(result != LDAP_SUCCESS)
  {
    std::cerr << "LDAP initialization failed: " << ldap_err2string(result) << std::endl;
    throw std::runtime_error("Error initializing LDAP");
  }

  int desired_version = LDAP_VERSION3;
  result = ldap_set_option(ldap_obj, LDAP_OPT_PROTOCOL_VERSION, &desired_version);

  if(result != LDAP_SUCCESS)
  {
    std::cerr << "Failed to set LDAP version " << ldap_err2string(result) << std::endl;
    ldap_unbind_ext_s(ldap_obj, nullptr, nullptr); // Cleanup
    throw std::runtime_error("Error setting LDAP version");
  }

  std::cout << "LDAP initialized and set to version 3\n";

  std::string dn = "uid=" + username + ",ou=People,dc=technikum-wien,dc=at";

  // cast password to LDAP-compatible BER-type
  BerValue cred;
  cred.bv_val = const_cast<char*>(password.c_str()); //const_cast to remove (const) bc ber-type expects non-const
  cred.bv_len = password.length();

  
  // Perform SASL bind using SIMPLE mechanism
  result = ldap_sasl_bind_s(
      ldap_obj,       // LDAP session handle
      dn.c_str(),     // Distinguished Name (DN)
      nullptr,        // Mechanism ("SIMPLE" uses nullptr)
      &cred,          // Pointer to credentials BerValue
      nullptr,        // Server controls (nullptr for none)
      nullptr,        // Client controls (nullptr for none)
      nullptr         // Pointer for result server credentials (not needed here)
  );

  if (result == LDAP_SUCCESS) {
      std::cout << "LDAP SASL bind successful for user: " << username << std::endl;
      loggedIn = true;
      authenticatedUser = username;
      send(consfd, "OK\n", 3, 0);
      return;
  } 

  // else error
  std::cerr << "LDAP SASL bind failed: " << ldap_err2string(result) << std::endl;
  send_error(consfd, "Invalid credentials in LOGIN");
  loggedIn = false;
  authenticatedUser.clear();
  attempted_logins_cnt++;
}

const void Server::send_error(const int consfd, const std::string errorMessage) {
  std::cout << "Send Error to Client - " << errorMessage << std::endl;
  send(consfd, "ERR\n", 4, 0);
}