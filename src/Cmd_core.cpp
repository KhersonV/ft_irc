#include "Channel.hpp"
#include "Cmd_core.hpp"
#include "Proto.hpp"
#include "State.hpp"
#include "utils.hpp"
#include <set>
#include <sstream>

using namespace ftirc;

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
	size_t	sp1;
	int		rfd;
	bool	first;
	size_t	pos;
	size_t	pos_trailing;
	bool	isOp;
	size_t	spk;

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
	if (cmd == "MODE")
	{
		return handle_MODE(fd, cl, clients, rest);
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
	if (cmd == "TOPIC")
	{
		if (!cl.registered)
		{
			send_numeric(clients, fd, 451, cl.nick.empty() ? "*" : cl.nick, "",
				"You have not registered");
			return (false);
		}
		if (rest.empty())
		{
			send_numeric(clients, fd, 461, cl.nick, "TOPIC",
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
		pos_trailing = rest.find(" :");
		if (pos_trailing == std::string::npos)
		{
			if (ch.topic.empty())
			{
				send_numeric(clients, fd, 331, cl.nick, chname,
					"No topic is set");
			}
			else
			{
				send_numeric(clients, fd, 332, cl.nick, chname, ch.topic);
				std::ostringstream p;
				p << chname << " " << (ch.topic_set_by.empty() ? "*" : ch.topic_set_by) << " " << (ch.topic_set_at ? ch.topic_set_at : 0);
				send_numeric(clients, fd, 333, cl.nick, p.str(), "");
			}
			return (false);
		}
		std::string new_topic = rest.substr(pos_trailing + 2);
		if (ch.topic_restricted && ch.ops.find(fd) == ch.ops.end())
		{
			send_numeric(clients, fd, 482, cl.nick, chname,
				"You're not channel operator");
			return (false);
		}
		ch.topic = new_topic;
		ch.topic_set_by = cl.nick;
		ch.topic_set_at = std::time(NULL);
		std::string line = ":" + cl.nick + " TOPIC " + ch.name + " :"
			+ new_topic;
		send_to_channel(clients, ch, line, -1);
		return (false);
	}
	if (cmd == "JOIN")
	{
		if (!cl.registered)
		{
			send_numeric(clients, fd, 451, cl.nick.empty() ? "*" : cl.nick, "",
				"You have not registered");
			return (false);
		}
		if (rest.empty())
		{
			send_numeric(clients, fd, 461, cl.nick, "JOIN",
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
		Channel &ch = g_state.channels[key];
		if (ch.name.empty())
			ch.name = chname;
		if (ch.invite_only && ch.invited.find(fd) == ch.invited.end())
		{
			send_numeric(clients, fd, 473, cl.nick, chname,
				"Cannot join channel (+i)");
			return (false);
		}
		spk = rest.find(' ');
		std::string provided_key;
		if (spk != std::string::npos)
		{
			std::string after = ltrim(rest.substr(spk + 1));
			provided_key = first_token(after);
		}
		if (!ch.key.empty() && ch.key != provided_key)
		{
			send_numeric(clients, fd, 475, cl.nick, chname,
				"Cannot join channel (+k)");
			return (false);
		}
		if (ch.user_limit > 0 && ch.members.size() >= (size_t)ch.user_limit)
		{
			send_numeric(clients, fd, 471, cl.nick, chname,
				"Cannot join channel (+l)");
			return (false);
		}
		first = ch.members.empty();
		ch.members.insert(fd);
		if (first)
			ch.ops.insert(fd);
		std::string joinMsg = ":" + cl.nick + " JOIN " + chname;
		send_to_channel(clients, ch, joinMsg, -1);
		return (false);
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
