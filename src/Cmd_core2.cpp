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

static void	leave_all_channels(std::map<int, Client> &clients,
		const std::string &nick, int fd, const std::string &reason)
{
	std::vector<std::string> to_erase;
	for (std::map<std::string,
		Channel>::iterator it = g_state.channels.begin(); it != g_state.channels.end(); ++it)
	{
		Channel &ch = it->second;
		if (ch.members.find(fd) != ch.members.end())
		{
			std::string line = ":" + nick + " QUIT :"
				+ (reason.empty() ? "Quit" : reason);
			send_to_channel(clients, ch, line, fd);
			remove_member_from_channel(ch, fd);
			if (ch.members.empty())
				to_erase.push_back(it->first);
		}
	}
	for (size_t i = 0; i < to_erase.size(); ++i)
	{
		g_state.channels.erase(to_erase[i]);
	}
}

// todo: extract each command into it's own function
// todo: add constants for all ints that describe unchangable fd's
bool	process_line(int fd, const std::string &line, std::map<int,
		Client> &clients, std::vector<int> &fds)
{
	size_t	sp;
	int		rfd;
	size_t	pos;
	size_t	pos_trailing;
	bool	isOp;

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
		if (!cl.registered)
		{
			send_numeric(clients, fd, 451, cl.nick.empty() ? "*" : cl.nick, "",
				"You have not registered");
			return (false);
		}
		// INVITE <nick> <#chan>
		std::string target = first_token(rest);
		std::string after = ltrim(rest.substr(target.size()));
		std::string chname = first_token(after);
		if (target.empty() || chname.empty())
		{
			send_numeric(clients, fd, 461, cl.nick, "INVITE",
				"Not enough parameters");
			return (false);
		}
		std::map<std::string,
			int>::iterator itNick = g_state.nick2fd.find(ftirc::lower_str(target));
		if (itNick == g_state.nick2fd.end())
		{
			send_numeric(clients, fd, 401, cl.nick, target, "No such nick");
			return (false);
		}
		std::map<std::string,
			Channel>::iterator itCh = g_state.channels.find(ftirc::lower_str(chname));
		if (itCh == g_state.channels.end())
		{
			send_numeric(clients, fd, 403, cl.nick, chname, "No such channel");
			return (false);
		}
		Channel &ch = itCh->second;
		if (!is_member(ch, fd))
		{
			send_numeric(clients, fd, 442, cl.nick, chname,
				"You're not on that channel");
			return (false);
		}
		if (ch.invite_only && !is_op(ch, fd))
		{
			send_numeric(clients, fd, 482, cl.nick, chname,
				"You're not channel operator");
			return (false);
		}
		rfd = itNick->second;
		ch.invited.insert(rfd);
		// 341 RPL_INVITING
		send_numeric(clients, fd, 341, cl.nick, target + " " + chname, "");
		// уведомим приглашённого
		enqueue_line(clients, rfd, ":" + cl.nick + " INVITE " + target + " :"
			+ chname);
		return (false);
	}
	if (cmd == "PART")
	{
		if (!cl.registered)
		{
			send_numeric(clients, fd, 451, cl.nick.empty() ? "*" : cl.nick, "",
				"You have not registered");
			return (false);
		}
		if (rest.empty())
		{
			send_numeric(clients, fd, 461, cl.nick, "PART",
				"Not enough parameters");
			return (false);
		}
		std::string chname = first_token(rest);
		if (chname.empty() || chname[0] != '#')
		{
			send_numeric(clients, fd, 403, cl.nick, chname, "No such channel");
			return (false);
		}
		std::string reason;
		pos_trailing = rest.find(" :");
		if (pos_trailing != std::string::npos)
			reason = rest.substr(pos_trailing + 2);
		std::string key = ftirc::lower_str(chname);
		std::map<std::string,
			Channel>::iterator itCh = g_state.channels.find(key);
		if (itCh == g_state.channels.end())
		{
			send_numeric(clients, fd, 403, cl.nick, chname, "No such channel");
			return (false);
		}
		Channel &ch = itCh->second;
		if (ch.members.find(fd) == ch.members.end())
		{
			send_numeric(clients, fd, 442, cl.nick, chname,
				"You're not on that channel");
			return (false);
		}
		std::string line = ":" + cl.nick + " PART " + ch.name;
		if (!reason.empty())
			line += " :" + reason;
		enqueue_line(clients, fd, line);
		send_to_channel(clients, ch, line, fd);
		remove_member_from_channel(ch, fd);
		if (ch.members.empty())
			g_state.channels.erase(key);
		return (false);
	}
	if (cmd == "NAMES")
	{
		if (!cl.registered)
		{
			send_numeric(clients, fd, 451, cl.nick.empty() ? "*" : cl.nick, "",
				"You have not registered");
			return (false);
		}
		if (rest.empty())
		{
			send_numeric(clients, fd, 461, cl.nick, "NAMES",
				"Not enough parameters");
			return (false);
		}
		std::string chname = first_token(rest);
		if (chname.empty() || chname[0] != '#')
		{
			send_numeric(clients, fd, 403, cl.nick, chname, "No such channel");
			return (false);
		}
		std::string key = ftirc::lower_str(chname);
		std::map<std::string,
			Channel>::iterator itCh = g_state.channels.find(key);
		if (itCh == g_state.channels.end())
		{
			send_numeric(clients, fd, 353, cl.nick, "= " + chname, "");
			send_numeric(clients, fd, 366, cl.nick, chname, "End of /NAMES list.");
			return (false);
		}
		Channel &ch = itCh->second;
		std::string names;
		for (std::set<int>::iterator it = ch.members.begin(); it != ch.members.end(); ++it)
		{
			const Client &m = clients[*it];
			if (!names.empty())
				names += " ";
			isOp = (ch.ops.find(*it) != ch.ops.end());
			names += (isOp ? "@" : "") + (m.nick.empty() ? "*" : m.nick);
		}
		send_numeric(clients, fd, 353, cl.nick, "= " + chname, names);
		send_numeric(clients, fd, 366, cl.nick, chname, "End of /NAMES list.");
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
			std::map<std::string,
				Channel>::iterator itCh = g_state.channels.find(ftirc::lower_str(target));
			if (itCh == g_state.channels.end())
			{
				send_numeric(clients, fd, 403, cl.nick, target,
					"No such channel");
				return (false);
			}
			Channel &ch = itCh->second;
			if (ch.members.find(fd) == ch.members.end())
			{
				send_numeric(clients, fd, 404, cl.nick, target,
					"Cannot send to channel");
				return (false);
			}
			std::string line = ":" + cl.nick + " PRIVMSG " + target + " :"
				+ text;
			send_to_channel(clients, ch, line, fd);
			return (false);
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
		std::string reason;
		if (!rest.empty())
		{
			if (rest[0] == ':')
				reason = rest.substr(1);
			else
			{
				pos = rest.find(" :");
				if (pos != std::string::npos)
					reason = rest.substr(pos + 2);
			}
		}
		leave_all_channels(clients, cl.nick.empty() ? "*" : cl.nick, fd,
			reason);
		ftirc::close_and_remove(fd, fds, clients);
		return (true);
	}
	enqueue_line(clients, fd, "You said: " + line);
	return (false);
}
