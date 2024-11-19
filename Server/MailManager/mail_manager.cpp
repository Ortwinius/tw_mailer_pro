#include "mail_manager.h"
#include <semaphore.h>
#include <fstream>
#include <iostream>
#include <sys/socket.h>
#include "../../utils/constants.h"
#include "../../utils/helpers.h"

MailManager::MailManager(const std::filesystem::path &mail_directory)
: mail_directory(mail_directory) 
{}

void MailManager::handle_list(int consfd, const std::string &authenticatedUser, sem_t *sem) {
    fs::path userInbox = mail_directory / authenticatedUser; // Path to the user's inbox
  if (!fs::exists(userInbox) || !fs::is_directory(userInbox)) {
    std::cout << "No messages or user unknown" << std::endl;
    send_server_response(consfd, "0\n", 2, 0);
    return;
  }

  int messageCount = 0;
  std::ostringstream response;

  // Iterate over the directory and gather subjects in one pass
  int counter = 0;
  sem_wait(sem);

  for (const auto &entry : fs::directory_iterator(userInbox)) {
    counter++;

    // lock Semaphore before iterating and reading through the files
    if (fs::is_regular_file(entry.path())) {
      std::ifstream messageFile(entry.path());
      std::string line;
      std::string subject;

      std::getline(messageFile, line);
      std::getline(messageFile, line); // skip sender and receiver

      if (std::getline(messageFile, subject)) {
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
  send_server_response(consfd, finalResponse.str().c_str(), finalResponse.str().size(), 0);
}

void MailManager::handle_send(int consfd, const std::string &buffer, const std::string &authenticatedUser, sem_t *sem) {
  std::string skipLine;
  std::string receiver;
  std::string subject;
  std::string message;

  std::istringstream iss(buffer);
  std::getline(iss, skipLine, '\n'); // skip command and content-length header
  std::getline(iss, skipLine, '\n');

  if (!std::getline(iss, receiver) || receiver.empty()) {
    std::cerr << "Invalid receiver in LIST" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
    return;
  }

  if (!std::getline(iss, subject) || subject.empty()) {
    std::cerr << "Invalid subject in LIST" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
    return;
  }

  std::string line;
  while (std::getline(iss, line)) {
    if (line == ".")
      break;
    else
      message += line + "\n";
  }

  fs::path receiverPath = mail_directory / receiver / (std::to_string(std::time(nullptr)) + "_" + authenticatedUser + ".txt");

  sem_wait(sem); // lock Semaphore before creating a directory and writing to a new file
  fs::create_directories(mail_directory / receiver);

  std::ofstream messageFile(receiverPath);
  if (messageFile.is_open()) {
    // Write the structured message
    messageFile << receiver << "\n";
    messageFile << subject << "\n";
    messageFile << message;

    messageFile.close();
    sem_post(sem); // unlock semaphore after closing messageFile

    std::cout << "Saved Mail " << subject << " in inbox of " << receiver << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_OK, 3, 0);
  } 
  else {
    sem_post(sem); // unlock semaphore if writing to file failed
    std::cerr << "Error while opening folder to save mail in SEND" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
  }
}

void MailManager::handle_read(int consfd, const std::string &buffer, const std::string &authenticatedUser, sem_t *sem) {
  std::string line;
  std::string messageNrString;

  std::istringstream iss(buffer);
  std::getline(iss, line, '\n'); // skip command and content-length header
  std::getline(iss, line, '\n');

  if (!std::getline(iss, messageNrString) || messageNrString.empty()) {
    std::cerr << "Invalid message number in READ" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
    return;
  }

  int messageNr = -1;
  try {
    messageNr = std::stoi(messageNrString);
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    std::cerr << "Error while parsing message number in READ" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
    return;
  }

  sem_wait(sem);                                           // Lock semaphore before checking file existance
  fs::path userInbox = mail_directory / authenticatedUser; // Path to the user's inbox
  if (!fs::exists(userInbox) || !fs::is_directory(userInbox)) {
    sem_post(sem); // Unlock semaphore if dir doesnt exist
    
    std::cerr << "No messages for user " + authenticatedUser + " in READ" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
    return;
  }

  int counter = 0;
  for (const auto &entry : fs::directory_iterator(userInbox)) {
    counter++;
    if (counter == messageNr) {
      std::ifstream messageFile(entry.path());
      if (!messageFile.is_open()) {
        std::cerr << "Unable to open message file in READ" << std::endl;
        send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
        return;
      }

      std::ostringstream messageContent;
      std::string line;

      while (std::getline(messageFile, line)) {
        messageContent << line << "\n";
      }

      sem_post(sem); // Unlock semaphore after reading the file

      std::string response = ServerConstants::RESPONSE_OK + messageContent.str() + "\n"; // Add an additional newline at the end
      send_server_response(consfd, response.c_str(), response.size(), 0);
      return;
    }
  }
  sem_post(sem); // unlock after iterator finished

  // invalid message number
  std::cerr << "Invalid message number in READ" << std::endl;
  send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
}

void MailManager::handle_delete(int consfd, const std::string &buffer, const std::string &authenticatedUser, sem_t *sem) {
  std::string line;
  std::string messageNrString;

  std::istringstream iss(buffer);
  std::getline(iss, line, '\n'); // skip command and content-length header
  std::getline(iss, line, '\n');

  if (!std::getline(iss, messageNrString) || messageNrString.empty()) {
    std::cerr << "Invalid message number in DEL" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
    return;
  }

  int messageNr = -1;
  try {
    messageNr = std::stoi(messageNrString);
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    std::cerr << "Error while parsing message number in DEL" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
    return;
  }

  fs::path userInbox = mail_directory / authenticatedUser; // Path to the user's inbox

  sem_wait(sem); // Lock semaphore before accessing the filesystem
  if (!fs::exists(userInbox) || !fs::is_directory(userInbox)) {
    std::cerr << "No messages or user unknown in DEL" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
    sem_post(sem); // Unlock if dir doesnt exist
    return;
  }

  int counter = 0;
  bool messageDeleted = false;
  for (const auto &entry : fs::directory_iterator(userInbox)) {
    counter++;
    if (counter == messageNr) {
      // Attempt to delete the message file
      if (fs::remove(entry.path())) {
        messageDeleted = true; // Mark as found if deletion was successful
        break;
      }
    }
  }
  sem_post(sem); // Unlock semaphore after deleting file

  if (messageDeleted) {
    send_server_response(consfd, ServerConstants::RESPONSE_OK, 3, 0); // Message deleted successfully
  } 
  else {
    std::cerr << "Error while deleting file in DEL" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
  }
}