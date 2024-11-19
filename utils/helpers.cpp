#include "helpers.h"
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>

void get_user_input(const std::string& prompt, std::string& buffer)
{
  std::cout << prompt;
  std::getline(std::cin, buffer);
}

void get_hidden_user_input(const std::string& prompt, std::string& buffer)
{
    std::cout << prompt;
    struct termios old_t_attr, new_t_attr;
    tcgetattr(STDIN_FILENO, &old_t_attr); // pulling current terminal attributes
    new_t_attr = old_t_attr; 
    new_t_attr.c_lflag &= ~ECHO; // disabling ECHO flag for hiding input 
    tcsetattr(STDIN_FILENO, TCSANOW, &new_t_attr);
    
    std::getline(std::cin, buffer); // read input into buffer
    tcsetattr(STDIN_FILENO, TCSANOW, &old_t_attr); // restore visibility by resetting terminal attributes 
    std::cout << "\n";
}

void resize_buffer(char *&buffer, ssize_t &currentSize, ssize_t newCapacity)
{
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

void send_server_response(int __fd, const void *buffer, size_t __n, int __flags)
{
  // add content length header at top
  int content_length = __n;
  std::string header_and_body = "Content-Length: " + std::to_string(content_length) + "\n";
  header_and_body.append(static_cast<const char*>(buffer), __n);
  // then send with send cmd
  send(__fd, header_and_body.c_str(), header_and_body.size(), __flags);
}
