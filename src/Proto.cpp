#include "Proto.hpp"
#include <sstream>

void enqueue_line(std::map<int, Client>& clients, int fd, const std::string& s)
{
	std::map<int, Client>::iterator it = clients.find(fd);
	if (it == clients.end()) return;
	it->second.out += s;
	if (it->second.out.size() < 2
		|| it->second.out.substr(it->second.out.size()-2) != "\r\n")
		it->second.out += "\r\n";
}

void send_numeric(std::map<int,Client>& clients, int fd, int code,
				  const std::string& nick,
				  const std::string& params,
				  const std::string& text)
{
	std::ostringstream oss;
	oss << ":ft_irc " << code << " " << (nick.empty() ? "*" : nick);
	if (!params.empty()) oss << " " << params;
	if (!text.empty())   oss << " :" << text;
	enqueue_line(clients, fd, oss.str());
}

void	send_to_channel(std::map<int, Client> &clients, const Channel &ch,
		const std::string &line, int except_fd)
{
	for (std::set<int>::iterator it = ch.members.begin(); it != ch.members.end(); ++it)
	{
		if (except_fd != -1 && *it == except_fd)
			continue ;
		enqueue_line(clients, *it, line);
	}
}