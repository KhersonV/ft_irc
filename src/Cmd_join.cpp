#include "Cmd_core.hpp"
#include "Proto.hpp"
#include "utils.hpp"
#include "Irc_codes.hpp"
#include "State.hpp"
#include <sstream>

using namespace ftirc;
using namespace ft_codes;

namespace
{

void send_topic_after_join(std::map<int, Client>& clients,
								  const Channel& ch, const Client& cl)
{
	if (!ch.topic.empty()) {
		send_numeric(clients, cl.fd, 332, cl.nick,
					 ch.name, ch.topic);
		if (!ch.topic_set_by.empty() && ch.topic_set_at > 0) {
			std::ostringstream oss;
			oss << ch.name << " " << ch.topic_set_by << " " << ch.topic_set_at;
			send_numeric(clients, cl.fd, 333, cl.nick,
						 oss.str(), "");
		}
	} else {
		send_numeric(clients, cl.fd, 331, cl.nick,
					 ch.name, "No topic is set");
	}
}

static void send_names_after_join(std::map<int, Client>& clients,
								  const Channel& ch, const Client& cl)
{
	std::string names;
	for (std::set<int>::const_iterator it = ch.members.begin();
		 it != ch.members.end(); ++it)
	{
		const Client& m = clients[*it];
		bool op = (ch.ops.find(*it) != ch.ops.end());
		if (!names.empty())
			names += " ";
		if (op)
			names += "@";
		names += (m.nick.empty() ? "*" : m.nick);
	}

	std::string params = "= " + ch.name + " :" + names;
	send_numeric(clients, cl.fd, 353, cl.nick, params, "");

	send_numeric(clients, cl.fd, 366, cl.nick,
				 ch.name, "End of NAMES list");
}

bool is_registered(Client &cl, int fd, std::map<int, Client> &clients)
{
	if (!cl.registered) {
		send_numeric(clients, fd, NOT_REGISTERED, cl.nick.empty() ? "*" : cl.nick, "",
					"You have not registered, register with NICK and USER");
		return false;
	}
	return true;
}

bool parse_join_args(const std::string &rest, std::string &chname, std::string &provided_key, const Client &cl, int fd, std::map<int, Client> &clients)
{
	if (rest.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "JOIN",
					"Usage: JOIN #channel [key]");
		return false;
	}

	chname = first_token(rest);
	if (chname.empty() || chname[0] != '#') {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, chname,
					"Specify a valid channel name starting with '#'");
		return false;
	}

	// Optional key = first token after first space
	std::string::size_type sp = rest.find(' ');
	if (sp != std::string::npos) {
		provided_key = first_token(ltrim(rest.substr(sp + 1)));
	} else {
		provided_key.clear();
	}
	return true;
}

Channel* get_or_create_channel(const std::string &chname)
{
	const std::string key = lower_str(chname);
	Channel &ch = g_state.channels[key];   // creates if missing
	if (ch.name.empty()) ch.name = chname; // initialize display name
	return &ch;
}

bool can_join_invite_only(const Channel &ch, const std::string &chname, const Client &cl, int fd, std::map<int, Client> &clients)
{
	if (ch.invite_only && ch.invited.find(fd) == ch.invited.end()) {
		send_numeric(clients, fd, INVITE_ONLY_CHANNEL, cl.nick, chname,
					"Cannot join channel (+i)");
		return false;
	}
	return true;
}

bool key_ok(const Channel &ch, const std::string &chname, const std::string &provided_key, const Client &cl, int fd, std::map<int, Client> &clients)
{
	if (!ch.key.empty() && ch.key != provided_key) {
		send_numeric(clients, fd, BAD_CHANNEL_KEY, cl.nick, chname,
					"Cannot join channel (+k)");
		return false;
	}
	return true;
}

bool under_user_limit(const Channel &ch, const std::string &chname, const Client &cl, int fd, std::map<int, Client> &clients)
{
	if (ch.user_limit > 0 && ch.members.size() >= (size_t)ch.user_limit) {
		send_numeric(clients, fd, CHANNEL_IS_FULL, cl.nick, chname,
					"Cannot join channel (+l)");
		return false;
	}
	return true;
}

inline void add_client_to_channel(Channel &ch, int fd)
{
	const bool first = ch.members.empty();
	ch.members.insert(fd);
	if (first) ch.ops.insert(fd);
}

inline void add_channel_to_client(Client &cl, Channel *ch)
{
	cl.channels.push_back(ch);
}

inline void broadcast_join(std::map<int, Client> &clients, Channel &ch, const Client &cl, const std::string &chname)
{
	const std::string line = ":" + cl.nick + " JOIN " + chname;
	send_to_channel(clients, ch, line, -1);
}
} // namespace

bool handle_JOIN(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest)
{
	if (!is_registered(cl, fd, clients))
		return false;

	std::string chname, provided_key;
	if (!parse_join_args(rest, chname, provided_key, cl, fd, clients))
		return false;

	Channel *ch = get_or_create_channel(chname);

	if (!can_join_invite_only(*ch, chname, cl, fd, clients))
		return false;
	if (!key_ok(*ch, chname, provided_key, cl, fd, clients))
		return false;
	if (!under_user_limit(*ch, chname, cl, fd, clients))
		return false;

	add_client_to_channel(*ch, fd);
	add_channel_to_client(cl, ch);
	ch->invited.erase(fd);
	broadcast_join(clients, *ch, cl, chname);
	send_topic_after_join(clients, *ch, cl);
	send_names_after_join(clients, *ch, cl);
	return false;
}
