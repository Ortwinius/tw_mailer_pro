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

void MailManager::handle_list(int consfd, const std::string &authenticated_user, sem_t *sem)
{
  fs::path user_inbox = mail_directory / authenticated_user; // Path to the user's inbox
  if (!fs::exists(user_inbox) || !fs::is_directory(user_inbox))
  {
    std::cout << "No messages or user unknown" << std::endl;
    send_server_response(consfd, "0\n", 2, 0);
    return;
  }

  int messageCount = 0;
  std::ostringstream response;

  // Iterate over the directory and gather subjects in one pass
  int counter = 0;
  sem_wait(sem);

  for (const auto &entry : fs::directory_iterator(user_inbox))
  {
    counter++;

    // lock Semaphore before iterating and reading through the files
    if (fs::is_regular_file(entry.path()))
    {
      std::ifstream message_file(entry.path());
      std::string line;
      std::string subject;

      std::getline(message_file, line); // skip receiver

      if (std::getline(message_file, subject))
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
  send_server_response(consfd, finalResponse.str().c_str(), finalResponse.str().size(), 0);
}

void MailManager::handle_send(int consfd, const std::string &buffer, const std::string &authenticated_user, sem_t *sem)
{
  std::string skipLine;
  std::string receiver;
  std::string subject;
  std::string message;

  std::istringstream iss(buffer);
  std::getline(iss, skipLine, '\n'); // skip command and content-length header
  std::getline(iss, skipLine, '\n');

  if (!std::getline(iss, receiver) || receiver.empty())
  {
    std::cout << "Invalid receiver in LIST" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
    return;
  }

  if (!std::getline(iss, subject) || subject.empty())
  {
    std::cout << "Invalid subject in LIST" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
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

  fs::path receiverPath = mail_directory / receiver / (std::to_string(std::time(nullptr)) + "_" + authenticated_user + ".txt");

  sem_wait(sem); // lock Semaphore before creating a directory and writing to a new file
  fs::create_directories(mail_directory / receiver);

  std::ofstream message_file(receiverPath);
  if (message_file.is_open())
  {
    // Write the structured message
    message_file << receiver << "\n";
    message_file << subject << "\n";
    message_file << message;

    message_file.close();
    sem_post(sem); // unlock semaphore after closing message_file

    std::cout << "Saved Mail " << subject << " in inbox of " << receiver << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_OK, 3, 0);
  }
  else
  {
    sem_post(sem); // unlock semaphore if writing to file failed
    std::cout << "Error while opening folder to save mail in SEND" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
  }
}

void MailManager::handle_read(int consfd, const std::string &buffer, const std::string &authenticated_user, sem_t *sem)
{
  std::string line;
  std::string message_nr_string;

  std::istringstream iss(buffer);
  std::getline(iss, line, '\n'); // skip command and content-length header
  std::getline(iss, line, '\n');

  if (!std::getline(iss, message_nr_string) || message_nr_string.empty())
  {
    std::cout << "Invalid message number in READ" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
    return;
  }

  int message_nr = -1;
  try
  {
    message_nr = std::stoi(message_nr_string);
  }
  catch (const std::exception &e)
  {
    std::cout << e.what() << '\n';
    std::cout << "Error while parsing message number in READ" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
    return;
  }

  sem_wait(sem);                                           // Lock semaphore before checking file existance
  fs::path user_inbox = mail_directory / authenticated_user; // Path to the user's inbox
  if (!fs::exists(user_inbox) || !fs::is_directory(user_inbox))
  {
    sem_post(sem); // Unlock semaphore if dir doesnt exist

    std::cout << "No messages for user " + authenticated_user + " in READ" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
    return;
  }

  int counter = 0;
  for (const auto &entry : fs::directory_iterator(user_inbox))
  {
    counter++;
    if (counter == message_nr)
    {
      std::ifstream message_file(entry.path());
      if (!message_file.is_open())
      {
        std::cout << "Unable to open message file in READ" << std::endl;
        send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
        return;
      }

      std::ostringstream message_content;
      std::string line;

      while (std::getline(message_file, line))
      {
        message_content << line << "\n";
      }

      sem_post(sem); // Unlock semaphore after reading the file

      std::string response = ServerConstants::RESPONSE_OK + message_content.str() + "\n"; // Add an additional newline at the end
      send_server_response(consfd, response.c_str(), response.size(), 0);
      return;
    }
  }
  sem_post(sem); // unlock after iterator finished

  // invalid message number
  std::cout << "Invalid message number in READ" << std::endl;
  send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
}

void MailManager::handle_delete(int consfd, const std::string &buffer, const std::string &authenticated_user, sem_t *sem)
{
  std::string line;
  std::string message_nr_string;

  std::istringstream iss(buffer);
  std::getline(iss, line, '\n'); // skip command and content-length header
  std::getline(iss, line, '\n');

  if (!std::getline(iss, message_nr_string) || message_nr_string.empty())
  {
    std::cout << "Invalid message number in DEL" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
    return;
  }

  int message_nr = -1;
  try
  {
    message_nr = std::stoi(message_nr_string);
  }
  catch (const std::exception &e)
  {
    std::cout << e.what() << '\n';
    std::cout << "Error while parsing message number in DEL" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
    return;
  }

  fs::path user_inbox = mail_directory / authenticated_user; // Path to the user's inbox

  sem_wait(sem); // Lock semaphore before accessing the filesystem
  if (!fs::exists(user_inbox) || !fs::is_directory(user_inbox))
  {
    std::cout << "No messages or user unknown in DEL" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
    sem_post(sem); // Unlock if dir doesnt exist
    return;
  }

  int counter = 0;
  bool message_deleted = false;
  for (const auto &entry : fs::directory_iterator(user_inbox))
  {
    counter++;
    if (counter == message_nr)
    {
      // Attempt to delete the message file
      if (fs::remove(entry.path()))
      {
        message_deleted = true; // Mark as found if deletion was successful
        break;
      }
    }
  }
  sem_post(sem); // Unlock semaphore after deleting file

  if (message_deleted)
  {
    send_server_response(consfd, ServerConstants::RESPONSE_OK, 3, 0); // Message deleted successfully
  }
  else
  {
    std::cout << "Error while deleting file in DEL" << std::endl;
    send_server_response(consfd, ServerConstants::RESPONSE_ERR, 4, 0);
  }
}