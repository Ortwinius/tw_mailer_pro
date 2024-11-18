#include "helpers.h"
#include <termios.h>
#include <unistd.h>
#include <cstring>

void getUserInput(const std::string& prompt, std::string& buffer)
{
  std::cout << prompt;
  std::getline(std::cin, buffer);
}

void getHiddenUserInput(const std::string& prompt, std::string& buffer)
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

void resizeBuffer(char *&buffer, ssize_t &currentSize, ssize_t newCapacity)
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