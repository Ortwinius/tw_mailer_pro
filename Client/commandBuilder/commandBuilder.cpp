#include "commandBuilder.h"
#include <iostream>
#include <sstream>

void CommandBuilder::addParameter(const std::string &param)
{
    parameters.push_back(param);
}

void CommandBuilder::addMessageContent()
{
    std::string line;
    std::cout << "Enter message content. End with a single '.' on a line by itself:\n";

    while(true)
    {
        std::getline(std::cin, line);
        if(line == ".")
        {
            messageLines.push_back(".\n");
            break;
        }
        messageLines.push_back(line + "\n");
    }
}

std::string CommandBuilder::buildFinalCommand(const std::string &commandName)
{
    std::ostringstream commandStream;

    commandStream << commandName << "\n";

    std::string body;
    // Add parameters (e.g. receiver etc.)
    for (const auto &param : parameters)
    {
        body += param + "\n";
    }

    // add body part if it exists (e.g. "SEND")
    for (const auto &line : messageLines) {
        body += line;
    }

    // add content length to the command
    commandStream << "Content-Length: " << body.length() << "\n" << body;
    return commandStream.str();
}
