#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <cctype>
#include <vector>
#include <map>
#include "Client.hpp"

namespace ftirc {

bool debug_lf_mode();

bool cut_line(std::string &ib, std::string &line, bool debugLF);

int set_nonblocking(int fd);

int create_listen_socket(int port);

void close_and_remove(int fd,
					  std::vector<int>& fds,
					  std::map<int, Client>& clients);

std::string to_upper(std::string s);
std::string lower_str(const std::string& nickname);
std::string ltrim(const std::string &s);
std::string first_token(const std::string &s);
std::vector<std::string> split(const std::string& s, char delimiter);

}

#endif
