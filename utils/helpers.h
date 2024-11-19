#ifndef HELPERS_H
#define HELPERS_H
#include <iostream>
#include <string>

// get input from user
void get_user_input(const std::string& prompt, std::string& buffer);
void get_hidden_user_input(const std::string& prompt, std::string& buffer);

void resize_buffer(char *&buffer, ssize_t &current_size, ssize_t new_capacity);

void send_server_response(int __fd, const void *buffer, size_t __n, int __flags);

bool check_content_length_header(std::string &content_length_header, int &content_length);

#endif