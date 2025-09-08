#include "Cmd_core.hpp"

using namespace ft_codes;
using namespace ftirc;
namespace
{

bool is_registered(Client &cl, int fd, std::map<int, Client> &clients)
{
	if (!cl.registered) {
		send_numeric(clients, fd, NOT_REGISTERED, cl.nick.empty() ? "*" : cl.nick, "",
					"You have not registered, register with NICK and USER");
		return false;
	}
	return true;
}

bool parse_names_args(const std::string &rest, std::string &chname, const Client &cl, int fd, std::map<int, Client> &clients)
{
	if (rest.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "NAMES",
					"Usage: NAMES #channel");
		return false;
	}
	chname = first_token(rest);
	if (chname.empty() || chname[0] != '#') {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, chname,
					"specify a valid channel with #<channel>");
		return false;
	}
	return true;
}

bool find_channel_silent(const std::string &chname, Channel *&out_ch)
{
	std::map<std::string,Channel>::iterator it =
		g_state.channels.find(lower_str(chname));
	if (it == g_state.channels.end()) {
		return false;
	}
	out_ch = &it->second;
	return true;
}

std::string build_names_list(const Channel &ch, const std::map<int, Client> &clients)
{
	std::string names;
	for (std::set<int>::const_iterator it = ch.members.begin(); it != ch.members.end(); ++it)
	{
		const Client &m = clients.find(*it)->second;
		if (!names.empty()) names += ' ';
		const bool isOp = (ch.ops.find(*it) != ch.ops.end());
		names += (isOp ? "@" : "");
		names += (m.nick.empty() ? "*" : m.nick);
	}
	return names;
}

inline void send_names_reply(std::map<int, Client> &clients, int fd, const Client &cl, const std::string &chname, const std::string &names)
{
	if (!names.empty()) { // don't send empty names list to not give away non-existence of channel
		send_numeric(clients, fd, NAMES_REPLY, cl.nick, "= " + chname, names);
	}
	send_numeric(clients, fd, END_OF_NAMES, cl.nick, chname, "End of /NAMES list.");
}

} // anonymous namespace

bool handle_NAMES(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest)
{
	if (!is_registered(cl, fd, clients))
		return false;

	std::string chname;
	if (!parse_names_args(rest, chname, cl, fd, clients))
		return false;

	Channel *ch = 0;
	if (!find_channel_silent(chname, ch)) { // find channel, but don't complain if missing
		send_names_reply(clients, fd, cl, chname, "");
		return false;
	}

	const std::string names = build_names_list(*ch, clients);
	send_names_reply(clients, fd, cl, chname, names);
	return false;
}