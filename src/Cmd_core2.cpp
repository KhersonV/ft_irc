#include "Channel.hpp"
#include "Cmd_core.hpp"
#include "Proto.hpp"
#include "State.hpp"
#include "utils.hpp"
#include <set>
#include <sstream>

using namespace ftirc;
using namespace ft_codes;

namespace
{
bool parse_input(const std::string &line, std::string &out_cmd, std::string &out_rest)
{
	std::string s(line);

	if (!s.empty() && s[0] == ':') {
		std::string::size_type sp = s.find(' ');
		if (sp != std::string::npos) s.erase(0, sp + 1);
		else return false;
	}

	std::string::size_type sp = s.find(' ');
	if (sp == std::string::npos) {
		out_cmd  = to_upper(s);
		out_rest.clear();
	} else {
		out_cmd  = to_upper(s.substr(0, sp));
		out_rest = s.substr(sp + 1);
	}
	return !out_cmd.empty();
}

}

void	finish_register(std::map<int, Client> &clients, int fd)
{
	Client &c = clients[fd];
	if (c.registered)
		return ;
	if (!g_state.server_password.empty() && !c.pass_ok)
		return ;
	if (c.nick.empty() || c.user.empty())
		return ;
	c.registered = true;
	send_numeric(clients, fd, WELCOME, c.nick, "", "Welcome to ft_irc " + c.nick);
	send_numeric(clients, fd, YOUR_HOST, c.nick, "", "Your host is ft_irc");
}

bool	process_line(int fd, const std::string &line, std::map<int,
		Client> &clients, std::vector<int> &fds)
{
	std::string cmd, rest;
	if (!parse_input(line, cmd, rest))
		return (false);

	Client &cl = clients[fd];
	if (cmd == "PASS")
	{
		return handle_PASS(fd, cl, clients, rest);
	} else if (!validate_PASS_OK(fd, cl, clients)) {
		return (false);
	}
	if (cmd == "NICK")
	{
		return handle_NICK(fd, cl, clients, rest);
	}
	if (cmd == "MODE")
	{
		return handle_MODE(fd, cl, clients, rest);
	}
	if (cmd == "USER")
	{
		return handle_USER(fd, cl, clients, rest);
	}
	if (cmd == "TOPIC")
	{
		return handle_TOPIC(fd, cl, clients, rest);
	}
	if (cmd == "JOIN")
	{
		return handle_JOIN(fd, cl, clients, rest);
	}
	if (cmd == "INVITE")
	{
		return handle_INVITE(fd, cl, clients, rest);
	}
	if (cmd == "PART")
	{
		return handle_PART(fd, cl, clients, rest);
	}
	if (cmd == "NAMES")
	{
		return handle_NAMES(fd, cl, clients, rest);
	}
	if (cmd == "PRIVMSG")
	{
		return handle_PRIVMSG(fd, cl, clients, rest);
	}
	if (cmd == "PING")
	{
		return handle_PING(fd, clients, rest);
	}
	if (cmd == "QUIT")
	{
		return handle_QUIT(fd, cl, clients, fds, rest);
	}
	enqueue_line(clients, fd, "You said: " + line);
	return (false);
}
