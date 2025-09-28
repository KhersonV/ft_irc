#include "Cmd_core.hpp"
#include "Proto.hpp"
#include "utils.hpp"
#include "Irc_codes.hpp"
#include "State.hpp"

using namespace ft_codes;
using namespace ftirc;

namespace
{

static std::string build_prefix_for_old_nick(const Client &cl, const std::string &old_nick)
{
	const std::string host = "localhost";
	const std::string user = cl.user.empty() ? old_nick : cl.user;
	const std::string nick = old_nick.empty() ? "*" : old_nick;
	return ":" + nick + "!" + user + "@" + host;
}

bool	is_valid_nick(const std::string &nickname)
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
		case ':':
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

bool extract_nick(const std::string &rest, std::string &out_nick, std::map<int, Client> &clients, int fd, const Client &cl)
{
	if (rest.empty()) {
		send_numeric(clients, fd, NO_NICKNAME_GIVEN, cl.nick, "",
					"No nickname given");
		return false;
	}
	std::string nick = rest;
	if (!nick.empty() && nick[0] == ':') nick.erase(0, 1);
	out_nick = first_token(nick);
	return true;
}

bool validate_nick(const std::string &nick, std::map<int, Client> &clients, int fd, const Client &cl)
{
	if (!is_valid_nick(nick)) {
		send_numeric(clients, fd, INVALID_NICKNAME, cl.nick, nick,
					"Nick contains invalid characters");
		return false;
	}
	return true;
}

bool is_nick_unique(const std::string &nick_key, int fd, std::map<int, Client> &clients,  int fd_for_msg, const Client &cl_for_msg, const std::string &nick_for_msg)
{
	std::map<std::string,int>::iterator it = g_state.nick2fd.find(nick_key);
	if (it != g_state.nick2fd.end() && it->second != fd) {
		send_numeric(clients, fd_for_msg, NICKNAME_IN_USE, cl_for_msg.nick,
					nick_for_msg, "Nickname is already in use");
		return false;
	}
	return true;
}

void remove_old_nick_mapping(Client &cl, int fd)
{
	if (!cl.registered)
	{
		if (!cl.nick.empty()) {
			std::string oldkey = lower_str(cl.nick);
			std::map<std::string,int>::iterator old = g_state.reservednick2fd.find(oldkey);
			if (old != g_state.reservednick2fd.end() && old->second == fd)
				g_state.reservednick2fd.erase(old);
		}
	}
	else {
		if (!cl.nick.empty()) {
			std::string oldkey = lower_str(cl.nick);
			std::map<std::string,int>::iterator old = g_state.nick2fd.find(oldkey);
			if (old != g_state.nick2fd.end() && old->second == fd)
				g_state.nick2fd.erase(old);
		}
	}
}

void commit_nick_update(Client &cl, int fd,  const std::string &nick, const std::string &nick_key)
{
	cl.nick = nick;
	if (cl.registered)
		g_state.nick2fd[nick_key] = fd;
	else
		g_state.reservednick2fd[nick_key] = fd;
}

void broadcast_nick_change(std::map<int, Client> &clients,
						   const Client &cl,
						   const std::string &new_nick,
						   const std::string &old_nick)
{
	if (old_nick.empty() || old_nick == new_nick) return;

	const std::string prefix = build_prefix_for_old_nick(cl, old_nick);
	const std::string msg = prefix + " NICK :" + new_nick;

	for (std::list<Channel*>::const_iterator it = cl.channels.begin();
		 it != cl.channels.end(); ++it)
	{
		send_to_channel(clients, **it, msg, cl.fd);
	}

	enqueue_line(clients, cl.fd, msg);
}
} // namespace

bool handle_NICK(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest)
{
	std::string nick;
	if (!extract_nick(rest, nick, clients, fd, cl))
		return false;

	if (!validate_nick(nick, clients, fd, cl))
		return false;

	std::string key = lower_str(nick);
	if (!is_nick_unique(key, fd, clients, fd, cl, nick))
		return false;

	std::string old_nick = cl.nick;
	remove_old_nick_mapping(cl, fd);
	commit_nick_update(cl, fd, nick, key);
	if (!old_nick.empty() || old_nick == nick) {
		broadcast_nick_change(clients, cl, nick, old_nick);
	}

	finish_register(clients, fd);
	return false;
}
