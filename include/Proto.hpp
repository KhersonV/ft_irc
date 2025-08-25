#ifndef PROTO_HPP
# define PROTO_HPP
# include "Client.hpp"
# include <map>
# include <string>

void	enqueue_line(std::map<int, Client> &clients, int fd,
			const std::string &s);

#endif
