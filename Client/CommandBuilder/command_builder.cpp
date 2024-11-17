#include "command_builder.h"
#include <iostream>
#include <sstream>

void CommandBuilder::add_parameter(const std::string &param)
{
    parameters.push_back(param);
}

void CommandBuilder::add_msg_content()
{
    std::string line;
    std::cout << "Enter message content. End with a single '.' on a line by itself:\n";

    while(true)
    {
        std::getline(std::cin, line);
        if(line == ".")
        {
            message_lines.push_back(".\n");
            break;
        }
        message_lines.push_back(line + "\n");
    }
}

std::string CommandBuilder::build_final_cmd(const std::string &cmd_name)
{
    std::ostringstream cmd_stream;

    cmd_stream << cmd_name << "\n";

    std::string body;
    // Add parameters (e.g. receiver etc.)
    for (const auto &param : parameters)
    {
        body += param + "\n";
    }

    // add body part if it exists (e.g. "SEND")
    for (const auto &line : message_lines) {
        body += line;
    }


    if (!body.empty() && body[0] == '\n') {
        body.erase(0, 1);
    }

    // add content length to the command
    cmd_stream << "Content-Length: " << body.length() << "\n" << body;
    return cmd_stream.str();
}
