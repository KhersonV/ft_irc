#include "Channel.hpp"
#include "Cmd_core.hpp"
#include "Proto.hpp"
#include "State.hpp"
#include "utils.hpp"
#include <set>
#include <sstream>

using namespace ftirc;

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
	send_numeric(clients, fd, 001, c.nick, "", "Welcome to ft_irc " + c.nick);
	send_numeric(clients, fd, 002, c.nick, "", "Your host is ft_irc");
}

// todo: extract each command into it's own function
// todo: add constants for all ints that describe unchangable fd's
bool	process_line(int fd, const std::string &line, std::map<int,
		Client> &clients, std::vector<int> &fds)
{
	size_t	sp;

	std::string s = line;
	if (!s.empty() && s[0] == ':')
	{
		sp = s.find(' ');
		if (sp != std::string::npos)
			s.erase(0, sp + 1);
		else
			s.clear();
	}
	sp = s.find(' ');
	std::string cmd = (sp == std::string::npos) ? s : s.substr(0, sp);
	std::string rest = (sp == std::string::npos) ? "" : s.substr(sp + 1);
	cmd = ftirc::to_upper(cmd);
	if (cmd.empty())
		return (false);
	Client &cl = clients[fd];
	if (cmd == "PASS")
	{
		return handle_PASS(fd, cl, clients, rest);
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
