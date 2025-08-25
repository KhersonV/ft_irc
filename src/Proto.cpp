#include "Proto.hpp"

void enqueue_line(std::map<int, Client>& clients, int fd, const std::string& s)
{
    std::map<int, Client>::iterator it = clients.find(fd);
    if (it == clients.end()) return;
    it->second.out += s;
    if (it->second.out.size() < 2
        || it->second.out.substr(it->second.out.size()-2) != "\r\n")
        it->second.out += "\r\n";
}
