#include "Cmd_core.hpp"
#include "utils.hpp"

bool handle_PING(int fd, Client &cl, std::map<int, Client> &clients, std::string &rest)
{
	if (!rest.empty() && rest[0] == ':')
		rest.erase(0, 1);
	if (rest.empty())
		rest = "ft_irc";
	enqueue_line(clients, fd, "PONG :" + rest);
	return (false);
}