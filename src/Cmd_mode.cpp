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

bool	handle_mode_key(Channel &ch, int fd, Client &cl, std::map<int,
		Client> &clients, const std::string &tok, const std::string &args)
{
	if (!must_be_member(ch, fd, clients, cl) || !must_be_op(ch, fd, clients,
			cl))
		return (false);
	if (tok == "+k")
	{
		std::string keyParam = first_token(args);
		if (keyParam.empty())
		{
			send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "MODE",
				"Not enough parameters");
			return (false);
		}
		if (!ch.key.empty())
		{
			send_numeric(clients, fd, KEY_ALREADY_SET, cl.nick, ch.name,
				"Channel key already set");
			return (false);
		}
		ch.key = keyParam;
		send_to_channel(clients, ch, ":" + cl.nick + " MODE " + ch.name + " +k *", -1);
		return (false);
	}
	else
	{ // "-k"
		if (!ch.key.empty())
		{
			ch.key.clear();
			send_to_channel(clients, ch, ":" + cl.nick + " MODE " + ch.name + " -k", -1);
		}
		return (false);
	}
}

bool	handle_mode_limit(Channel &ch, int fd, Client &cl, std::map<int,
		Client> &clients, const std::string &tok, const std::string &args)
{
	int	n;

	if (!must_be_member(ch, fd, clients, cl) || !must_be_op(ch, fd, clients,
			cl))
		return (false);
	if (tok == "+l")
	{
		std::string nParam = first_token(args);
		if (nParam.empty())
		{
			send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "MODE",
				"Not enough parameters");
			return (false);
		}
		std::istringstream iss(nParam);
		n = 0;
		iss >> n;
		if (!iss || n <= 0)
		{
			send_numeric(clients, fd, UNKNOWN_MODE, cl.nick, "l",
				"Invalid limit");
			return (false);
		}
		ch.user_limit = n;
		std::ostringstream ss;
		ss << n;
		send_to_channel(clients, ch, ":" + cl.nick + " MODE " + ch.name + " +l "
			+ ss.str(), -1);
		return (false);
	}
	else // "-l"
	{
		if (ch.user_limit != 0)
		{
			ch.user_limit = 0;
			send_to_channel(clients, ch, ":" + cl.nick + " MODE " + ch.name + " -l", -1);
		}
		return (false);
	}
}

bool handle_mode_op(Channel &ch, int fd, Client &cl,
					std::map<int, Client> &clients,
					const std::string &tok,
					const std::string &args)
{
	if (!must_be_member(ch, fd, clients, cl) || !must_be_op(ch, fd, clients,
			cl))
		return (false);

	std::string targetNick = first_token(args);
	if (targetNick.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "MODE", "Not enough parameters");
		return false;
	}

	std::string keyNick = ftirc::lower_str(targetNick);
	std::map<std::string, int>::iterator iterator_fd = g_state.nick2fd.find(keyNick);
	if (iterator_fd == g_state.nick2fd.end()) {
		send_numeric(clients, fd, NO_SUCH_NICK, cl.nick, targetNick, "No such nick");
		return false;
	}
	int target_fd = iterator_fd->second;

	// the target should be member of the channel
	if (!is_member(ch, target_fd)) {
			send_numeric(clients, fd, USER_NOT_IN_CHANNEL, cl.nick,
						targetNick + " " + ch.name,
						"They aren't on that channel");
			return false;
	}

	bool give = (tok == "+o");
	bool changed = false;

	if (give) {
		if (ch.ops.insert(target_fd).second)
			changed = true;
	} else {
		std::set<int>::iterator op_iterator = ch.ops.find(target_fd);
		if (op_iterator != ch.ops.end()) {
			ch.ops.erase(op_iterator);
			changed = true;
		}
	}

	if (changed) {
		std::string line = ":" + cl.nick + " MODE " + ch.name + " "
							+ (give ? "+o " : "-o ") + targetNick;
		send_to_channel(clients, ch, line, -1);
	}
	return false;
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
	std::string tok = first_token(tail);
	std::string args = ltrim(tail.substr(tok.size()));
	if (!tail.empty())
	{
		if (tok == "+t" || tok == "-t")
			return (handle_mode_topic(ch, fd, cl, clients, tok));
		if (tok == "+i" || tok == "-i")
			return (handle_mode_invite(ch, fd, cl, clients, tok));
		if (tok == "+k" || tok == "-k")
			return (handle_mode_key(ch, fd, cl, clients, tok, args));
		if (tok == "+l" || tok == "-l")
			return handle_mode_limit(ch, fd, cl, clients, tok, args);
		if (tok == "+o" || tok == "-o")
			return handle_mode_op(ch, fd, cl, clients, tok, args);

		// если не распознали
		send_numeric(clients, fd, UNKNOWN_MODE, cl.nick, tok,
			"unknown mode char; use [+-]<t,i,k,l,o>");
		return (false);
	}
	// показать текущие режимы
	std::string modes = "+";
	if (ch.topic_restricted)
		modes += 't';
	if (ch.invite_only)
		modes += 'i';
	if (!ch.key.empty())
		modes += 'k';
	if (ch.user_limit > 0)
		modes += 'l';
	std::string modeParams;
	if (!ch.key.empty())
		modeParams += " *";
	if (ch.user_limit > 0)
	{
		std::ostringstream ss; ss << ch.user_limit;
		modeParams += " " + ss.str();
	}
	send_numeric(clients, fd, CHANNEL_MODE_IS, cl.nick, ch.name + " " + modes
		+ modeParams, "");
	return (false);
}
