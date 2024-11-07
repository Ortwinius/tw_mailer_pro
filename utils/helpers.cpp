#include "helpers.h"

void getUserInput(const std::string& prompt, std::string& buffer)
{
    std::cout << prompt;
    std::getline(std::cin, buffer);
}