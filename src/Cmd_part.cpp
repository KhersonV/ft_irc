#include "Cmd_core.hpp"
#include "Channel.hpp"
#include "Proto.hpp"
#include "State.hpp"
#include "utils.hpp"
#include "Irc_codes.hpp"

using namespace ftirc;
using namespace ft_codes;
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

bool parse_part_args(const std::string &rest, std::string &chname, std::string &reason, const Client &cl, int fd, std::map<int, Client> &clients)
{
	if (rest.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "PART",
					"Usage: PART #channel [:reason]");
		return false;
	}

	chname = first_token(rest);
	if (chname.empty() || chname[0] != '#') {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, chname, "specify a valid channel with #<channel>");
		return false;
	}

	std::string::size_type pos_trailing = rest.find(" :");
	if (pos_trailing != std::string::npos)
		reason = rest.substr(pos_trailing + 2);
	else
		reason.clear();

	return true;
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

bool is_member_of_channel(const Channel &ch, const std::string &chname, const Client &cl, int fd, std::map<int, Client> &clients)
{
	if (!is_member(ch, fd)) {
		send_numeric(clients, fd, NOT_ON_CHANNEL, cl.nick, chname, "You're not on that channel");
		return false;
	}
	return true;
}

inline void broadcast_part(std::map<int, Client> &clients, Channel &ch, const Client &cl, int fd, const std::string &chname, const std::string &reason)
{
	std::string line = ":" + cl.nick + " PART " + chname;
	if (!reason.empty()) line += " :" + reason;
	enqueue_line(clients, fd, line);
	send_to_channel(clients, ch, line, fd);
}

inline void remove_channel_from_client(Client &cl, Channel *ch)
{
	cl.channels.remove(ch);
}

// Remove user and delete channel if empty.
inline void remove_and_maybe_delete(const std::string &chname, Channel &ch, int fd)
{
	remove_member_from_channel(ch, fd);
	if (ch.members.empty())
		g_state.channels.erase(lower_str(chname));
}

} // anonymous namespace

bool handle_PART(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest)
{
	if (!is_registered(cl, fd, clients))
		return false;

	std::string chname, reason;
	if (!parse_part_args(rest, chname, reason, cl, fd, clients))
		return false;

	Channel *ch = 0;
	if (!find_channel(chname, ch, cl, fd, clients))
		return false;

	if (!is_member_of_channel(*ch, chname, cl, fd, clients))
		return false;

	broadcast_part(clients, *ch, cl, fd, chname, reason);
	remove_and_maybe_delete(chname, *ch, fd); // removes user from channel
	remove_channel_from_client(cl, ch); // removes channel from user's list
	return false;
}