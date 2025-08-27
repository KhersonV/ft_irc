#include "Cmd_core.hpp"
#include "Proto.hpp"
#include "State.hpp"
#include "utils.hpp"
#include "Channel.hpp"
#include <set>


static bool	is_valid_nick(const std::string &nickname)
{
	unsigned char	c;

	if (nickname.empty() || nickname.size() > 30)
	{
		return (false);
	}
	for (size_t i = 0; i < nickname.size(); ++i)
	{
		c = nickname[i];
		if (std::isalnum(c))
		{
			continue ;
		}
		switch (c)
		{
		case '-':
		case '_':
		case '[':
		case ']':
		case '\\':
		case '`':
		case '^':
		case '{':
		case '}':
			continue ;
		default:
			return (false);
		}
	}
	return (true);
}

static void	finish_register(std::map<int, Client> &clients, int fd)
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

static std::string first_token(const std::string &s)
{
	std::string::size_type ws = s.find(' ');
	return (ws == std::string::npos) ? s : s.substr(0, ws);
}

static void	send_to_channel(std::map<int, Client> &clients, const Channel &ch,
		const std::string &line, int except_fd)
{
	for (std::set<int>::iterator it = ch.members.begin(); it != ch.members.end(); ++it)
	{
		if (except_fd != -1 && *it == except_fd)
			continue ;
		enqueue_line(clients, *it, line);
	}
}

bool	process_line(int fd, const std::string &line, std::map<int,
		Client> &clients, std::vector<int> &fds)
{
	size_t	sp;
	size_t	sp1;
	size_t	pos_trailing;
	int		rfd;

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
		if (cl.registered)
		{
			send_numeric(clients, fd, 462, cl.nick, "",
				"You may not reregister");
			return (false);
		}
		if (rest.empty())
		{
			send_numeric(clients, fd, 461, cl.nick, "PASS",
				"Not enough parameters");
			return (false);
		}
		std::string pass = rest;
		std::string::size_type ws = pass.find(' ');
		if (ws != std::string::npos)
			pass.erase(ws);
		if (!pass.empty() && pass[0] == ':')
			pass.erase(0, 1);
		if (!g_state.server_password.empty() && pass != g_state.server_password)
		{
			send_numeric(clients, fd, 464, cl.nick, "", "Password incorrect");
			return (false);
		}
		cl.pass_ok = true;
		finish_register(clients, fd);
		return (false);
	}
	if (cmd == "NICK")
	{
		if (rest.empty())
		{
			send_numeric(clients, fd, 431, cl.nick, "", "No nickname given");
			return (false);
		}
		std::string nick = rest;
		if (!nick.empty() && nick[0] == ':')
			nick.erase(0, 1);
		nick = first_token(nick);
		if (!is_valid_nick(nick))
		{
			send_numeric(clients, fd, 432, cl.nick, nick,
				"Nick contains invalid characters");
			return (false);
		}
		std::string key = ftirc::lower_str(nick);
		if (g_state.nick2fd.find(key) != g_state.nick2fd.end()
			&& g_state.nick2fd[key] != fd)
		{
			send_numeric(clients, fd, 433, cl.nick, nick,
				"Nickname is already in use");
			return (false);
		}
		if (!cl.nick.empty())
		{
			std::map<std::string,
				int>::iterator old = g_state.nick2fd.find(ftirc::lower_str(cl.nick));
			if (old != g_state.nick2fd.end() && old->second == fd)
				g_state.nick2fd.erase(old);
		}
		cl.nick = nick;
		g_state.nick2fd[key] = fd;
		finish_register(clients, fd);
		return (false);
	}
	if (cmd == "USER")
	{
		if (cl.registered)
		{
			send_numeric(clients, fd, 462, cl.nick, "",
				"You may not reregister");
			return (false);
		}
		if (rest.empty())
		{
			send_numeric(clients, fd, 461, cl.nick, "USER",
				"Not enough parameters");
			return (false);
		}
		std::string username = rest;
		sp1 = username.find(' ');
		if (sp1 != std::string::npos)
		{
			username.erase(sp1);
		}
		std::string realname;
		pos_trailing = rest.find(" :");
		if (pos_trailing != std::string::npos)
		{
			realname = rest.substr(pos_trailing + 2);
		}
		else
		{
			send_numeric(clients, fd, 461, cl.nick, "USER",
				"Not enough parameters");
			return (false);
		}
		if (username.empty())
		{
			send_numeric(clients, fd, 461, cl.nick, "USER",
				"Not enough parameters");
			return (false);
		}
		cl.user = username;
		cl.realname = realname;
		finish_register(clients, fd);
		return (false);
	}
	if (cmd == "PRIVMSG")
	{
		if (!cl.registered)
		{
			send_numeric(clients, fd, 451, cl.nick.empty() ? "*" : cl.nick, "",
				"You have not registered");
			return (false);
		}
		if (rest.empty())
		{
			send_numeric(clients, fd, 411, cl.nick, "PRIVMSG",
				"No recipient given (PRIVMSG)");
			return (false);
		}
		std::string target = rest;
		sp = target.find(' ');
		if (sp != std::string::npos)
			target.erase(sp);
		if (target.empty())
		{
			send_numeric(clients, fd, 411, cl.nick, "PRIVMSG",
				"No recipient given (PRIVMSG)");
			return (false);
		}
		pos_trailing = rest.find(" :");
		if (pos_trailing == std::string::npos)
		{
			send_numeric(clients, fd, 412, cl.nick, "", "No text to send");
			return (false);
		}
		std::string text = rest.substr(pos_trailing + 2);

		if (!target.empty() && target[0] == '#')
		{
			std::map<std::string, Channel>::iterator itCh =
				g_state.channels.find(ftirc::lower_str(target));
			if (itCh == g_state.channels.end())
			{
				send_numeric(clients, fd, 403, cl.nick, target, "No such channel");
				return false;
			}
			Channel &ch = itCh->second;
			if (ch.members.find(fd) == ch.members.end())
			{
				send_numeric(clients, fd, 404, cl.nick, target,
					"Cannot send to channel");
				return false;
			}
			std::string line = ":" + cl.nick + " PRIVMSG " + target + " :" + text;
			send_to_channel(clients, ch, line, fd);
			return false;
		}

		std::map<std::string,
			int>::iterator itNick = g_state.nick2fd.find(ftirc::lower_str(target));
		if (itNick == g_state.nick2fd.end())
		{
			send_numeric(clients, fd, 401, cl.nick, target, "No such nick");
			return (false);
		}
		rfd = itNick->second;
		std::string line = ":" + (cl.nick.empty() ? "*" : cl.nick) + " PRIVMSG "
			+ target + " :" + text;
		enqueue_line(clients, rfd, line);
		return (false);
	}
	if (cmd == "PING")
	{
		if (!rest.empty() && rest[0] == ':')
			rest.erase(0, 1);
		if (rest.empty())
			rest = "ft_irc";
		enqueue_line(clients, fd, "PONG :" + rest);
		return (false);
	}
	if (cmd == "QUIT")
	{
		ftirc::close_and_remove(fd, fds, clients);
		return (true);
	}
	enqueue_line(clients, fd, "You said: " + line);
	return (false);
}
