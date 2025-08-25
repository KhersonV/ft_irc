
#ifndef CMD_CORE_HPP
# define CMD_CORE_HPP
# include "Client.hpp"
# include <map>
# include <string>
# include <vector>

bool	process_line(int fd, const std::string &line, std::map<int,
			Client> &clients, std::vector<int> &fds);

#endif
