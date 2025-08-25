#ifndef PROTO_HPP
# define PROTO_HPP
# include "Client.hpp"
# include <map>
# include <string>

void	enqueue_line(std::map<int, Client> &clients, int fd,
			const std::string &s);

void send_numeric(std::map<int,Client>& clients, int fd, int code,
				  const std::string& nick,
				  const std::string& params,
				  const std::string& text);
#endif
