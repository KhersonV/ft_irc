#include "Channel.hpp"
#include "Cmd_core.hpp"
#include "Irc_codes.hpp"
#include "Proto.hpp"
#include "State.hpp"
#include "utils.hpp"
#include <sstream>

using namespace	ftirc;
using namespace	ft_codes;

bool	must_be_member(const Channel &ch, int fd, std::map<int,
		Client> &clients, const Client &cl)
{
	if (!is_member(ch, fd))
	{
		send_numeric(clients, fd, NOT_ON_CHANNEL, cl.nick, ch.name,
			"You're not on that channel");
		return (false);
	}
	return (true);
}

bool	must_be_op(const Channel &ch, int fd, std::map<int, Client> &clients,
		const Client &cl)
{
	if (!is_op(ch, fd))
	{
		send_numeric(clients, fd, CHAN_OP_PRIVS_NEEDED, cl.nick, ch.name,
			"You're not channel operator");
		return (false);
	}
	return (true);
}

bool	handle_mode_topic(Channel &ch, int fd, Client &cl, std::map<int,
		Client> &clients, const std::string &tok)
{
	bool	new_val;

	if (!must_be_member(ch, fd, clients, cl) || !must_be_op(ch, fd, clients,
			cl))
		return (false);
	new_val = (tok == "+t");
	if (ch.topic_restricted != new_val)
	{
		ch.topic_restricted = new_val;
		send_to_channel(clients, ch, ":" + cl.nick + " MODE " + ch.name + " "
			+ tok, -1);
	}
	return (false);
}

bool	handle_mode_invite(Channel &ch, int fd, Client &cl, std::map<int,
		Client> &clients, const std::string &tok)
{
	bool	new_val;

	if (!must_be_member(ch, fd, clients, cl) || !must_be_op(ch, fd, clients,
			cl))
		return (false);
	new_val = (tok == "+i");
	if (ch.invite_only != new_val)
	{
		ch.invite_only = new_val;
		send_to_channel(clients, ch, ":" + cl.nick + " MODE " + ch.name + " "
			+ tok, -1);
	}
	return (false);
}

bool	handle_MODE(int fd, Client &cl, std::map<int, Client> &clients,
		const std::string &rest)
{
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
	std::string rest2 = ltrim(rest);
	std::string chname = first_token(rest2);
	if (chname.empty() || chname[0] != '#')
	{
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "MODE",
			"Channel name must start with '#'");
		return (false);
	}
	std::map<std::string,
		Channel>::iterator it = g_state.channels.find(ftirc::lower_str(chname));
	if (it == g_state.channels.end())
	{
		send_numeric(clients, fd, NO_SUCH_CHANNEL, cl.nick, chname,
			"No such channel");
		return (false);
	}
	Channel &ch = it->second;
	std::string tail = (rest2.size() > chname.size()) ? ltrim(rest2.substr(chname.size())) : std::string();
	if (!tail.empty())
	{
		std::string tok = first_token(tail);
		if (tok == "+t" || tok == "-t")
			return (handle_mode_topic(ch, fd, cl, clients, tok));
		if (tok == "+i" || tok == "-i")
			return (handle_mode_invite(ch, fd, cl, clients, tok));
		// если не распознали
		send_numeric(clients, fd, UNKNOWN_MODE, cl.nick, tok,
			"unknown mode char; use [+-]<t,i>");
		return (false);
	}
	// показать текущие режимы
	std::string modes = "+";
	if (ch.topic_restricted)
		modes += 't';
	if (ch.invite_only)
		modes += 'i';
	send_numeric(clients, fd, CHANNEL_MODE_IS, cl.nick, ch.name + " " + modes,
		"");
	return (false);
}
