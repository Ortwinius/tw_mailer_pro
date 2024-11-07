#ifndef COMMANDBUILDER
#define COMMANDBUILDER

#include <iostream>
#include <vector>

class CommandBuilder {
public:
    CommandBuilder() = default;
    ~CommandBuilder() = default;
    
    void addParameter(const std::string& param);
    void addMessageContent();
    std::string buildFinalCommand(const std::string &commandName);
    
private:
    std::vector<std::string> parameters;
    std::vector<std::string> messageLines;
};

#endif