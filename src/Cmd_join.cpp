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

bool parse_join_args(const std::string &rest, std::vector<std::string> &chnames, std::vector<std::string> &provided_keys, const Client &cl, int fd, std::map<int, Client> &clients)
{
	if (rest.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "JOIN",
					"Usage: JOIN #channel [key]");
		return false;
	}

	std::string channels_part = first_token(rest);
	chnames = split(channels_part, ',');

	if (chnames.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "JOIN",
					"Specify at least one channel starting with '#'");
		return false;
	}

	for (std::vector<std::string>::const_iterator it = chnames.begin(); it != chnames.end(); ++it)
	{
		if (it->empty() || (*it)[0] != '#') {
			// fail the whole command if any channel is invalid
			send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, channels_part,
						"Specify valid channel names starting with '#'");
			return false;
		}
	}

	provided_keys.clear();
	provided_keys.resize(chnames.size());

	// Optional keys part: first token after first space
	std::string::size_type sp = rest.find(' ');
	if (sp != std::string::npos) {
		std::string keys_part = first_token(ltrim(rest.substr(sp + 1)));
		if (!keys_part.empty()) {
			std::vector<std::string> keys = split(keys_part, ',');
			// less keys may be provided that channels, that's ok
			const std::size_t limit = keys.size() < provided_keys.size() ? keys.size() : provided_keys.size();
			for (std::size_t i = 0; i < limit; ++i) {
				provided_keys[i] = keys[i]; // may be empty to intentionally skip a key (keyA,,keyC)
			}
		}
	}

	return true;
}

// note: creator becomes op, but in add_client_to_channel(Channel &ch, int fd)
Channel* get_or_create_channel(const std::string &chname, const std::string &key)
{
	Channel &ch = g_state.channels[lower_str(chname)];   // creates if missing
	if (ch.name.empty()) // new channel
	{
		ch.name = chname;
		if (!key.empty()) ch.key = key; // set key if provided on chan creation
	}
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

	std::vector<std::string> chnames, provided_keys;
	if (!parse_join_args(rest, chnames, provided_keys, cl, fd, clients))
		return false;

	for (std::size_t i = 0; i < chnames.size(); ++i) {
		const std::string& chname = chnames[i];
		const std::string& provided_key = provided_keys[i];

		Channel* ch = get_or_create_channel(chname, provided_key);
		if (!ch) { // should never happen, just a safe-guard
			send_numeric(clients, fd, NO_SUCH_CHANNEL, cl.nick, chname, "Unable to create or access channel");
			continue; // try next channel
		}

		if (!can_join_invite_only(*ch, chname, cl, fd, clients))
			continue;

		if (!key_ok(*ch, chname, provided_key, cl, fd, clients))
			continue;

		if (!under_user_limit(*ch, chname, cl, fd, clients))
			continue;

		// Perform the join
		add_client_to_channel(*ch, fd);
		add_channel_to_client(cl, ch);
		ch->invited.erase(fd);

		broadcast_join(clients, *ch, cl, chname);
		send_topic_after_join(clients, *ch, cl);
		send_names_after_join(clients, *ch, cl);
	}
	return false;
}
