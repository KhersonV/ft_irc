#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>
#include <map>

namespace ftirc {

bool debug_lf_mode();

bool cut_line(std::string &ib, std::string &line, bool debugLF);

int set_nonblocking(int fd);

int create_listen_socket(int port);

void close_and_remove(int fd,
                      std::vector<int>& clients,
                      std::map<int, std::string>& inbuf,
                      std::map<int, std::string>& outbuf);

} 

#endif 
