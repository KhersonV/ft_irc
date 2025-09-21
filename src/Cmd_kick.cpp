#include "Cmd_core.hpp"
#include "Proto.hpp"
#include "utils.hpp"
#include "Irc_codes.hpp"
#include "State.hpp"

using namespace ftirc;
using namespace ft_codes;

namespace {

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

	bool find_channel(const std::string &chname, Channel *&out, const Client &cl, int fd, std::map<int, Client> &clients)
	{
		std::map<std::string, Channel>::iterator it = g_state.channels.find(lower_str(chname));
		if (it == g_state.channels.end()) {
			send_numeric(clients, fd, NO_SUCH_CHANNEL, cl.nick, chname, "No such channel");
			return false;
		}
		out = &it->second;
		return true;
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

	bool handle_KICK(int fd, Client &cl, std::map<int, Client> &clients,
		const std::string &rest)
	{
		return 1;

	}
}

