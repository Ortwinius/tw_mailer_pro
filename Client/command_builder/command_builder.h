#ifndef COMMANDBUILDER
#define COMMANDBUILDER

#include <iostream>
#include <vector>

class CommandBuilder {
public:
    CommandBuilder() = default;
    ~CommandBuilder() = default;
    
    void add_parameter(const std::string& param);
    void add_msg_content();
    std::string build_final_cmd(const std::string &commandName);
    
private:
    std::vector<std::string> parameters;
    std::vector<std::string> message_lines;
};

#endif