#ifndef HELPERS_H
#define HELPERS_H
#include <iostream>
#include <string>

// get input from user
void getUserInput(const std::string& prompt, std::string& buffer);
void getHiddenUserInput(const std::string& prompt, std::string& buffer);

void resizeBuffer(char *&buffer, ssize_t &currentSize, ssize_t newCapacity);

#endif