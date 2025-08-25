#ifndef NET_HPP
# define NET_HPP

# include "Client.hpp"
# include <map>
# include <vector>

void	handle_write_ready(int fd, std::map<int, Client> &clients,
			std::vector<int> &fds);

bool	handle_read_ready(int fd, std::map<int, Client> &clients,
			std::vector<int> &fds);

void	add_client(int cfd, std::vector<int> &fds, std::map<int,
			Client> &clients);

#endif
