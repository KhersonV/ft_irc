#include "Channel.hpp"
#include "Cmd_core.hpp"
#include "Irc_codes.hpp"
#include "Proto.hpp"
#include "State.hpp"
#include "utils.hpp"
#include <sstream>

using namespace	ftirc;
using namespace	ft_codes;

bool	handle_MODE(int fd, Client &cl, std::map<int, Client> &clients,
		const std::string &rest)
{
	bool	new_val;
	char	bad;

	if (!cl.registered)
	{
		send_numeric(clients, fd, NOT_REGISTERED, cl.nick, "",
			"Not registered");
		return (false);
	}
	if (rest.empty())
	{
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "MODE",
			"Not enough parameters");
		return (false);
	}
	// trim leading spaces; rest2 is used to avoid modifying `rest`
	std::string rest2 = ltrim(rest);
	std::string chname = first_token(rest2);
	if (chname.empty())
	{
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "MODE",
			"Not enough parameters");
		return (false);
	}
	if (chname[0] != '#')
	{
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "MODE",
			"Channel name must start with '#'");
		return (false);
	}
	std::string key = ftirc::lower_str(chname);
	std::map<std::string, Channel>::iterator it = g_state.channels.find(key);
	if (it == g_state.channels.end())
	{
		send_numeric(clients, fd, NO_SUCH_CHANNEL, cl.nick, chname,
			"No such channel");
		return (false);
	}
	Channel &ch = it->second;
	// everything after channel name (possible mode string)
	std::string tail = (rest2.size() > chname.size()) ? ltrim(rest2.substr(chname.size())) : std::string();
	if (!tail.empty())
	{
		// ------- +t / -t (topic restriction)
		if (tail == "+t" || tail == "-t")
		{
			if (!is_member(ch, fd))
			{
				send_numeric(clients, fd, 442, cl.nick, ch.name,
					"You're not on that channel");
				return (false);
			}
			if (!is_op(ch, fd))
			{
				send_numeric(clients, fd, 482, cl.nick, ch.name,
					"You're not channel operator");
				return (false);
			}
			new_val = (tail == "+t");
			if (ch.topic_restricted != new_val)
			{
				ch.topic_restricted = new_val;
				std::string line = ":" + cl.nick + " MODE " + ch.name + " "
					+ tail;
				send_to_channel(clients, ch, line, -1);
			}
			return (false);
		}
		// ------- +i / -i (invite-only)
		if (tail == "+i" || tail == "-i")
		{
			if (!is_member(ch, fd))
			{
				send_numeric(clients, fd, 442, cl.nick, ch.name,
					"You're not on that channel");
				return (false);
			}
			if (!is_op(ch, fd))
			{
				send_numeric(clients, fd, 482, cl.nick, ch.name,
					"You're not channel operator");
				return (false);
			}
			new_val = (tail == "+i");
			if (ch.invite_only != new_val)
			{
				ch.invite_only = new_val;
				std::string line = ":" + cl.nick + " MODE " + ch.name + " "
					+ tail;
				send_to_channel(clients, ch, line, -1);
			}
			return (false);
		}
		// неизвестный режим — оставляем как было
		bad = 0;
		for (size_t i = 0; i < tail.size(); ++i)
		{
			if (tail[i] != ' ' && tail[i] != '\t' && tail[i] != '+'
				&& tail[i] != '-')
			{
				bad = tail[i];
				break ;
			}
		}
		std::string bad_s(1, bad ? bad : '?');
		send_numeric(clients, fd, UNKNOWN_MODE, cl.nick, bad_s,
			"unknown mode char; use [+-]<t,i>");
		return (false);
	}
	// MODE #chan  -> показать текущие режимы (без изменения)
	std::string modes = "+";
	if (ch.topic_restricted)
		modes += 't';
	if (ch.invite_only)
		modes += 'i';
	if (!ch.key.empty())
		modes += 'k';
	if (ch.user_limit > 0)
		modes += 'l';
	if (modes == "+")
		modes = '+'; // сохранить поведение
	std::string modeParams;
	if (!ch.key.empty())
	{
		modeParams += " *";
	}
	if (ch.user_limit > 0)
	{
		std::ostringstream ss;
		ss << ch.user_limit;
		modeParams += " " + ss.str();
	}
	std::string params = ch.name + " " + modes + modeParams;
	send_numeric(clients, fd, CHANNEL_MODE_IS, cl.nick, params, "");
	return (false);
}
