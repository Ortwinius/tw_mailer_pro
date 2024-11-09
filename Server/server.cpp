#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <stdexcept>
#include <cstring>
#include <fstream>
#include <sstream>
#include <semaphore.h>
#include "server.h"

namespace fs = std::filesystem;

Server::Server(int port, const fs::path &mailDirectory) 
: port(port)
, mail_directory(mailDirectory)
{
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
    if (socket_fd == -1)
    {
        throw std::runtime_error("Error initializing the server socket: " + std::to_string(errno));
    }
    
    std::cout << "Socket was created with id " << socket_fd << "\n";

    // set up server address struct to bind socket to a specific address
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY; // Accept connections from any interface
    serveraddr.sin_port = htons(port);

    // set socket options (SO_REUSEADDRE - reusing local address and same port even if in TIME_WAIT)
    int enable = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1)
    {
        throw std::runtime_error("Unable to set socket options due to: " + std::to_string(errno));
    }

    // binds the socket with a specific address
    if (bind(socket_fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1)
    {
        throw std::runtime_error("Binding failed with error: " + std::to_string(errno));
    }

    // socket starts listening for connection requests
    if (listen(socket_fd, 6) == -1)
    {
        throw std::runtime_error("Unable to establish connection: " + std::to_string(errno));
    }
}

void Server::listen_for_connections()
{
    int pid_t;

    // init semaphore
    sem_t sem;
    sem_init(&sem, 1, 1);

    while (true)
    {
        struct sockaddr client;
        socklen_t addrlen = sizeof(client);
        int peersoc = accept(socket_fd, &client, &addrlen);
        if (peersoc == -1)
        {
            std::cerr << "Unable to accept client connection.\n";
            continue; // Continue accepting other connections
        }
        pid_t =fork();
        if (pid_t < 0) // error
        {
            std::cerr << "Error: Fork failed" << std::endl;
            exit(EXIT_FAILURE);
        }
        else if (pid_t == 0) // child process
        {
            std::cout << "Accepted connection with file descriptor: " << peersoc << "\n";
            handle_communication(peersoc, &sem);
            exit(EXIT_SUCCESS);
        }
        else // parent process
        {
            std::cout << "Parent process: forked child " << pid_t << " to handle communication with file descriptor  " << peersoc << "\n";
        }

    }
}

void Server::handle_communication(int consfd, sem_t *sem)
{
    ssize_t bufferSize = 64;
    char* buffer = new char[bufferSize];

    bool validFormat=true;
    bool loggedIn=false;
    std::string authenticatedUser="";

    while (true)
    {
        ssize_t totalReceived = recv(consfd, buffer, bufferSize - 1, 0);
        if (totalReceived <= 0)
        {
            std::cerr << "Receive error or connection closed.\n";
            break;
        }

        buffer[totalReceived] = '\0'; // Null-terminate the received message at currentSize (counted by recv)

        std::string message(buffer);

        std::istringstream stream(message); // create stream for msg buffer 
        std::string command;
        std::getline(stream, command);    // get Command (first line)

        std::string contentLengthHeader;
        std::getline(stream, contentLengthHeader);
        int contentLength;

        int headerAndCommandLength = command.length() + contentLengthHeader.length() + ServerConstants::newLineCharacterSize; // +2 for \n in each line

        // check if contentLengthHeader is correct Format(content-length: <length>), get length
        if(checkContentLengthHeader(contentLengthHeader, contentLength))
        {
            if(contentLength+headerAndCommandLength > totalReceived)
            {
                resizeBuffer(buffer, bufferSize, headerAndCommandLength + contentLength + 1);

                ssize_t remaining = bufferSize - totalReceived;
                ssize_t received = recv(consfd, &buffer[totalReceived], remaining, 0);

                totalReceived += received;

                buffer[totalReceived] = '\0';

                //std::cout<<"TotalReceived: "<<totalReceived<<" | Buffer size: "<<bufferSize<<std::endl;
                message = std::string(buffer);
            }
        }
        else
        {
            validFormat = false;
        }

        std::cout << "Received: " << buffer << "\n";

        if(!validFormat)
        {
            send_error(consfd, "Message has invalid format"); // Respond with an error message
        }

        // QUIT to close conn
        if (command=="QUIT")
        {
            std::cout << "Closing connection with client [FileDescriptor: " << consfd << "]" << std::endl;
            close(consfd);
            break;
        }
        if(command=="LOGIN")
        {
            std::cout << "Processing LOGIN command" << std::endl;
            handle_login(consfd, buffer, authenticatedUser, loggedIn);            
        }
        else if(loggedIn)
        {
            if (command=="SEND")
            {
                std::cout << "Processing SEND command" << std::endl;
                handle_send(consfd, buffer, authenticatedUser, sem);
            }
            else if (command=="LIST")
            {
                std::cout << "Processing LIST command" << std::endl;
                handle_list(consfd, authenticatedUser, sem);
            }
            else if (command=="READ")
            {
                std::cout << "Processing READ command" << std::endl;
                handle_read(consfd, buffer, authenticatedUser, sem);
            }
            else if (command=="DEL")
            {
                std::cout << "Processing DEL command" << std::endl;
                handle_delete(consfd, buffer, authenticatedUser, sem);
            }
            else
            {
                send_error(consfd, "Message has unknown command"); // Respond with an error message
            }
        }
        else
        {
            std::cout<<"User unauthorized" << std::endl;
            send(consfd, "Unauthorized\n", 13, 0);
        }
    }
    delete[] buffer; // Free the buffer memory
    close(consfd);   // Ensure the peer socket is closed
}

void Server::resizeBuffer(char*& buffer, ssize_t& currentSize, ssize_t newCapacity) {
    if (newCapacity <= currentSize) {
        return;  // No resizing needed if the new size is smaller or equal
    }

    // Allocate new memory
    char* newBuffer = new char[newCapacity];

    // Copy old data into the new buffer
    std::memcpy(newBuffer, buffer, currentSize);

    // Free the old buffer
    delete[] buffer;

    // Update the buffer pointer and size
    buffer = newBuffer;
    currentSize=newCapacity;
}

bool Server::checkContentLengthHeader(std::string &contentLengthHeader, int &contentLength)
{
    if (contentLengthHeader.rfind("Content-Length:", 0) == 0) // Starts with "content-length:"
    {
        // get the actual length
        std::string lengthStr = contentLengthHeader.substr(15);
        lengthStr.erase(0, lengthStr.find_first_not_of(" \t")); // Trim leading spaces
        lengthStr.erase(lengthStr.find_last_not_of(" \t") + 1);

        try
        {
            contentLength = std::stoul(lengthStr); // Convert to size_t
        }
        catch (const std::invalid_argument &)
        {
            std::cout << "Invalid content-length value." << std::endl;
            return false;
        }
        catch (const std::out_of_range &)
        {
            std::cout << "Content-length value out of range." << std::endl;
            return false;
        }
    }
    else
    {
        std::cout << "Invalid format" << std::endl;
        return false;
    }
    return true;
}

void Server::handle_login(int consfd, const std::string &buffer, std::string &authenticatedUser, bool &loggedIn)
{
    std::string line;
    std::string username;
    std::string password;

    std::istringstream iss(buffer);

    std::getline(iss, line, '\n'); // skip command and content-length header
    std::getline(iss, line, '\n');

    if (!std::getline(iss, username) || username.empty())
    {
        send_error(consfd,"Invalid sender in LOGIN");
        return;
    }

    if (!std::getline(iss, password) || password.empty())
    {
        send_error(consfd, "Invalid password in LOGIN");
        return;
    }


    // TODO
    // Check if user+password is valid in LDAP, then
    // loggedIn=true;
    // authenticatedUser=username;
    // else
    // {
    //     send_error(consfd, "Invalid credentials in LOGIN");
    // }


    // To delete
    loggedIn = true;
    authenticatedUser = username;
    send(consfd, "OK\n", 3, 0);
}

void Server::handle_list(int consfd, const std::string &authenticatedUser, sem_t *sem)
{
    fs::path userInbox = mail_directory / authenticatedUser; // Path to the user's inbox
    if (!fs::exists(userInbox) || !fs::is_directory(userInbox))
    {
        std::cout << "No messages or user unknown" << std::endl;
        send(consfd, "0\n", 2, 0);
        return;
    }

    int messageCount = 0;
    std::ostringstream response;

    // Iterate over the directory and gather subjects in one pass
    int counter = 0;
    sem_wait(sem);
    for (const auto &entry : fs::directory_iterator(userInbox))
    {
        counter++;

        // lock Semaphore before iterating and reading through the files
        if (fs::is_regular_file(entry.path()))
        {
            std::ifstream messageFile(entry.path());
            std::string line;
            std::string subject;

            std::getline(messageFile, line);
            std::getline(messageFile, line); // skip sender and receiver

            if (std::getline(messageFile, subject))
            {
                messageCount++;
                response << "[" << counter << "] " << subject << "\n"; // Add subject to the response
            }
        }
    }
    sem_post(sem);

    // Construct the final response with the count first
    std::ostringstream finalResponse;
    finalResponse << messageCount << "\n"; // Add the message count first
    finalResponse << response.str();       // Append all subjects

    // Send the complete response
    send(consfd, finalResponse.str().c_str(), finalResponse.str().size(), 0);
}

void Server::handle_send(int consfd, const std::string &buffer, const std::string &authenticatedUser, sem_t *sem)
{
    std::string skipLine;
    std::string receiver;
    std::string subject;
    std::string message;

    std::istringstream iss(buffer);
    std::getline(iss, skipLine, '\n'); // skip command and content-length header
    std::getline(iss, skipLine, '\n');


    // NOTE: evtl. is_valid_username entfernen/anpassen -> kommt drauf an wie die Namen im LDAP sind

    if (!std::getline(iss, receiver) || receiver.empty() || (!is_valid_username(receiver)))
    {
        send_error(consfd, "Invalid receiver in SEND");
        return;
    }

    if (!std::getline(iss, subject) || subject.empty())
    {
        send_error(consfd, "Invalid subject in SEND");
        return;
    }

    std::string line;
    while (std::getline(iss, line))
    {
        if (line == ".")
            break;
        else
            message += line + "\n";
    }

    fs::path receiverPath = mail_directory / receiver / (std::to_string(std::time(nullptr)) + "_" + authenticatedUser + ".txt");

    sem_wait(sem); // lock Semaphore before creating a directory and writing to a new file
    fs::create_directories(mail_directory / receiver);

    std::ofstream messageFile(receiverPath);
    if (messageFile.is_open())
    {
        // Write the structured message
        messageFile << authenticatedUser << "\n";
        messageFile << receiver << "\n";
        messageFile << subject << "\n";
        messageFile << message;

        messageFile.close();
        sem_post(sem); // unlock semaphore after closing messageFile

        std::cout << "Saved Mail " << subject << " in inbox of " << receiver << std::endl;
        send(consfd, "OK\n", 3, 0);
    }
    else
    {
        sem_post(sem);// unlock semaphore if writing to file failed
        send_error(consfd, "Error when opening folder to save mail");
    }
}

void Server::handle_read(int consfd, const std::string &buffer, const std::string &authenticatedUser, sem_t *sem)
{
    std::string line;
    std::string messageNrString;

    std::istringstream iss(buffer);
    std::getline(iss, line, '\n'); // skip command and content-length header
    std::getline(iss, line, '\n');

    if (!std::getline(iss, messageNrString) || messageNrString.empty())
    {
        send_error(consfd, "Invalid Message number in READ");
        return;
    }

    int messageNr=-1;
    try
    {
        messageNr = std::stoi(messageNrString);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        send_error(consfd, "Error when parsing Message number to int in READ");
        return;
    }

    sem_wait(sem); // Lock semaphore before checking file existance
    fs::path userInbox = mail_directory / authenticatedUser; // Path to the user's inbox
    if (!fs::exists(userInbox) || !fs::is_directory(userInbox))
    {
        sem_post(sem); //Unlock semaphore if dir doesnt exist
        send_error(consfd,"No messages for user "+authenticatedUser+" in READ" );
        return;
    }

    int counter=0;
    for (const auto &entry : fs::directory_iterator(userInbox))
    {
        counter++;
        if (counter==messageNr)
        {
            std::ifstream messageFile(entry.path());
            if (!messageFile.is_open())
            {
                send_error(consfd, "Unable to open message file in READ"); // Unable to open the message file
                return;
            }

            std::ostringstream messageContent;
            std::string line;

            while(std::getline(messageFile, line))
            {
                messageContent<<line<<"\n";
            }

            sem_post(sem); // Unlock semaphore after reading the file

            std::string response = "OK\n" + messageContent.str() + "\n"; // Add an additional newline at the end
            send(consfd, response.c_str(), response.size(), 0);
            return;
        }
    }
    sem_post(sem); // unlock after iterator finished

    // invalid message number
    send_error(consfd, "Invalid Message number in READ");
}

void Server::handle_delete(int consfd, const std::string &buffer, const std::string &authenticatedUser, sem_t *sem)
{
    std::string line;
    std::string messageNrString;

    std::istringstream iss(buffer);
    std::getline(iss, line, '\n'); // skip command and content-length header
    std::getline(iss, line, '\n');

    if (!std::getline(iss, messageNrString) || messageNrString.empty())
    {
        send_error(consfd, "Invalid message number in DEL");
        return;
    }

    int messageNr = -1;
    try
    {
        messageNr = std::stoi(messageNrString);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        send_error(consfd, "Error when parsing message number to int in DEL");
        return;
    }



    fs::path userInbox = mail_directory / authenticatedUser; // Path to the user's inbox

    sem_wait(sem); // Lock semaphore before accessing the filesystem
    if (!fs::exists(userInbox) || !fs::is_directory(userInbox))
    {
        send_error(consfd, "No messages or User unknown");
        sem_post(sem); // Unlock if dir doesnt exist
        return;
    }

    int counter = 0;
    bool messageDeleted = false;
    for (const auto &entry : fs::directory_iterator(userInbox))
    {
        counter++;
        if (counter == messageNr)
        {
            // Attempt to delete the message file
            if (fs::remove(entry.path()))
            {
                messageDeleted = true; // Mark as found if deletion was successful
                break;
            }
        }
    }
    sem_post(sem); //Unlock semaphore after deleting file

    if (messageDeleted)
    {
        send(consfd, "OK\n", 3, 0); // Message deleted successfully
    }
    else
    {
        send_error(consfd, "Failed to delete file in DEL"); // Message number does not exist
    }

}

const bool Server::is_valid_username(const std::string &name) 
{
    // Prüfen, ob die Länge des Strings größer als 8 ist
    if (name.length() > 8) {
        return false;
    }

    // Jeden Charakter im String checken ob 0-9 oder a-z
    for (const char &c : name) 
    {
        if (!(isdigit(c) || (islower(c)))) 
        {
            return false;
        }
    }

    return true;
}

const void Server::send_error(const int consfd, const std::string errorMessage)
{
    std::cout<<"Send Error to Client - "<<errorMessage <<std::endl;
    send(consfd, "ERR\n", 4, 0);
}
