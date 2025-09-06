#include "Cmd_core.hpp"
#include "Proto.hpp"
#include "utils.hpp"
#include "Irc_codes.hpp"
#include "State.hpp"

using namespace ft_codes;
using namespace ftirc;

namespace
{
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
} // namespace

bool handle_NICK(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest)
{
	if (rest.empty()) {
		send_numeric(clients, fd, NO_NICKNAME_GIVEN, cl.nick, "",
					"No nickname given");
		return false;
	}

	std::string nick = rest;
	if (!nick.empty() && nick[0] == ':') nick.erase(0, 1);
	nick = first_token(nick);

	if (!is_valid_nick(nick)) {
		send_numeric(clients, fd, INVALID_NICKNAME, cl.nick, nick,
					"Nick contains invalid characters");
		return false;
	}

	std::string key = lower_str(nick);
	std::map<std::string,int>::iterator it = g_state.nick2fd.find(key);
	if (it != g_state.nick2fd.end() && it->second != fd) {
		send_numeric(clients, fd, ft_codes::NICKNAME_IN_USE, cl.nick, nick,
					"Nickname is already in use");
		return false;
	}

	// Remove old mapping (if any) for this fd
	if (!cl.nick.empty()) {
		std::string oldkey = lower_str(cl.nick);
		std::map<std::string,int>::iterator old = g_state.nick2fd.find(oldkey);
		if (old != g_state.nick2fd.end() && old->second == fd)
			g_state.nick2fd.erase(old);
	}

	cl.nick = nick;
	g_state.nick2fd[key] = fd;

	finish_register(clients, fd);
	return false;
}