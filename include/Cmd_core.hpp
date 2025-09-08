
#ifndef CMD_CORE_HPP
# define CMD_CORE_HPP
# include "Client.hpp"
# include <map>
# include <string>
# include <vector>
# include "Channel.hpp"
# include "State.hpp"
# include "utils.hpp"
# include "Irc_codes.hpp"
# include "Proto.hpp"

bool	process_line(int fd, const std::string &line, std::map<int,
			Client> &clients, std::vector<int> &fds);

bool handle_PING(int fd, std::map<int, Client> &clients, std::string &rest);
bool handle_PASS(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest);
bool handle_MODE(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest);
bool handle_NICK(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest);
bool handle_USER(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest);
bool handle_TOPIC(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest);
bool handle_JOIN(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest);
bool handle_QUIT(int fd, Client &cl,  std::map<int, Client> &clients, std::vector<int> &fds, const std::string &rest);

void finish_register(std::map<int, Client> &clients, int fd);

#endif
