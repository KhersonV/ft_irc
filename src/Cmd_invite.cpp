#include "Cmd_core.hpp"
#include "Irc_codes.hpp"
#include "Proto.hpp"
#include "State.hpp"
#include "utils.hpp"

using namespace	ftirc;
using namespace	ft_codes;

namespace
{
bool	is_registered(Client &cl, int fd, std::map<int, Client> &clients)
{
	if (!cl.registered)
	{
		send_numeric(clients, fd, NOT_REGISTERED,
			cl.nick.empty() ? "*" : cl.nick, "", "You have not registered, register with NICK and USER");
		return (false);
	}
	return (true);
}

bool	parse_invite_args(const std::string &rest, std::string &target,
		std::string &chname, const Client &cl, int fd, std::map<int,
		Client> &clients)
{
	// INVITE <nick> <#chan>
	target = first_token(rest);
	std::string after = ltrim(rest.substr(target.size()));
	chname = first_token(after);
	if (target.empty() || chname.empty())
	{
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "INVITE",
			"Usage: INVITE <nick> <#channel>");
		return (false);
	}
	return (true);
}

bool	lookup_target_fd(const std::string &target, int &out_fd,
		const Client &cl, int fd, std::map<int, Client> &clients)
{
	std::map<std::string,
		int>::iterator it = g_state.nick2fd.find(lower_str(target));
	if (it == g_state.nick2fd.end())
	{
		send_numeric(clients, fd, NO_SUCH_NICK, cl.nick, target,
			"No such nick");
		return (false);
	}
	out_fd = it->second;
	return (true);
}

bool	find_channel(const std::string &chname, Channel *&out_ch,
		const Client &cl, int fd, std::map<int, Client> &clients)
{
	std::map<std::string,
		Channel>::iterator it = g_state.channels.find(lower_str(chname));
	if (it == g_state.channels.end())
	{
		send_numeric(clients, fd, NO_SUCH_CHANNEL, cl.nick, chname,
			"No such channel");
		return (false);
	}
	out_ch = &it->second;
	return (true);
}

bool	is_member_of_channel(const Channel &ch, const std::string &chname,
		const Client &cl, int fd, std::map<int, Client> &clients)
{
	if (!is_member(ch, fd))
	{
		send_numeric(clients, fd, NOT_ON_CHANNEL, cl.nick, chname,
			"You're not on that channel");
		return (false);
	}
	return (true);
}

bool	ensure_invite_privs(Channel &ch, const std::string &chname,
		const Client &cl, int fd, std::map<int, Client> &clients)
{
	if (ch.invite_only && !is_op(ch, fd))
	{
		send_numeric(clients, fd, CHAN_OP_PRIVS_NEEDED, cl.nick, chname,
			"You're not channel operator");
		return (false);
	}
	return (true);
}

inline void	record_invite(Channel &ch, int target_fd)
{
	ch.invited.insert(target_fd);
}

inline void	reply_inviting(std::map<int, Client> &clients, int fd,
		const Client &cl, const std::string &target, const std::string &chname)
{
	send_numeric(clients, fd, INVITING, cl.nick, target + " " + chname, "");
}

inline void	notify_invited(std::map<int, Client> &clients, int rfd,
		const Client &cl, const std::string &target, const std::string &chname)
{
	enqueue_line(clients, rfd, ":" + cl.nick + " INVITE " + target + " :"
		+ chname);
}

} // anonymous namespace

// --- handler ---
bool	handle_INVITE(int fd, Client &cl, std::map<int, Client> &clients,
		const std::string &rest)
{
	if (!is_registered(cl, fd, clients))
		return (false);

	std::string target, chname;
	if (!parse_invite_args(rest, target, chname, cl, fd, clients))
		return (false);

	int rfd = -1;
	if (!lookup_target_fd(target, rfd, cl, fd, clients))
		return (false);

	Channel *ch = 0;
	if (!find_channel(chname, ch, cl, fd, clients))
		return (false);

	if (!is_member_of_channel(*ch, chname, cl, fd, clients))
		return false;

	if (!ensure_invite_privs(*ch, chname, cl, fd, clients))
		return false;

	if (ch->members.find(rfd) != ch->members.end())
	{
		send_numeric(clients, fd, USER_ON_CHANNEL, cl.nick, target + " " + chname,
			"is already on channel");
		return false;
	}

	record_invite(*ch, rfd);
	reply_inviting(clients, fd, cl, target, chname);
	notify_invited(clients, rfd, cl, target, chname);
	return false;
}
