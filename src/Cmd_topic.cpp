#include "Cmd_core.hpp"
#include "Channel.hpp"
#include "Proto.hpp"
#include "Irc_codes.hpp"
#include "utils.hpp"
#include "State.hpp"
#include <sstream>

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
bool is_member(const Channel &ch, const std::string &chname, int fd, const Client &cl, std::map<int, Client> &clients)
{
	if (ch.members.find(fd) == ch.members.end()) {
		send_numeric(clients, fd, ft_codes::NOT_ON_CHANNEL, cl.nick, chname,
					"You're not on that channel");
		return false;
	}
	return true;
}

bool can_set_topic(const Channel &ch, const std::string &chname, int fd, const Client &cl, std::map<int, Client> &clients)
{
	if (ch.topic_restricted && !is_op(ch, fd)) {
		send_numeric(clients, fd, ft_codes::CHAN_OP_PRIVS_NEEDED, cl.nick, chname,
					"You're not channel operator");
		return false;
	}
	return true;
}

bool parse_topic_target(const std::string &rest, std::string &chname, const Client &cl, int fd, std::map<int, Client> &clients)
{
	if (rest.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "TOPIC",
					 "Usage: TOPIC #channel [:new topic]");
		return false;
	}
	chname = first_token(rest);
	if (chname.empty() || chname[0] != '#') {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, chname,
					 "Channel name must start with '#'");
		return false;
	}
	return true;
}

bool find_channel(const std::string &chname, Channel *&out_ch, const Client &cl, int fd, std::map<int, Client> &clients)
{
	const std::string key = lower_str(chname);
	std::map<std::string, Channel>::iterator it = g_state.channels.find(key);
	if (it == g_state.channels.end()) {
		send_numeric(clients, fd, NO_SUCH_CHANNEL, cl.nick, chname,
					 "No such channel");
		return false;
	}
	out_ch = &it->second;
	return true;
}

inline std::string extract_new_topic(const std::string &rest) {
	std::string::size_type pos = rest.find(" :");
	return (pos == std::string::npos) ? std::string() : rest.substr(pos + 2);
}

inline void apply_topic_change(Channel &ch, const Client &cl, const std::string &new_topic)
{
	ch.topic = new_topic;
	ch.topic_set_by = cl.nick;
	ch.topic_set_at = std::time(NULL);
}

inline void announce_topic_change(std::map<int, Client> &clients, Channel &ch, const Client &cl, const std::string &new_topic)
{
	std::string line = ":" + cl.nick + " TOPIC " + ch.name + " :" + new_topic;
	send_to_channel(clients, ch, line, -1);
}

inline bool is_show_topic_request(const std::string &rest) {
	return rest.find(" :") == std::string::npos;
}

void reply_topic_info(const Channel &ch, const std::string &chname, const Client &cl, int fd, std::map<int, Client> &clients) {
	if (ch.topic.empty()) {
		send_numeric(clients, fd, NO_TOPIC, cl.nick, chname, "No topic is set");
	} else {
		send_numeric(clients, fd, TOPIC, cl.nick, chname, ch.topic);
		std::ostringstream p;
		p << chname << " "
		  << (ch.topic_set_by.empty() ? "*" : ch.topic_set_by) << " "
		  << (ch.topic_set_at ? ch.topic_set_at : 0);
		send_numeric(clients, fd, TOPIC_WHO_TIME, cl.nick, p.str(), "");
	}
}

} // namespace

bool handle_TOPIC(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest)
{
	if (!is_registered(cl, fd, clients))
		return false;

	std::string chname;
	if (!parse_topic_target(rest, chname, cl, fd, clients))
		return false;

	Channel *ch;
	if (!find_channel(chname, ch, cl, fd, clients))
		return false;

	if (!is_member(*ch, chname, fd, cl, clients))
		return false;


	if (is_show_topic_request(rest)) { // is it a show or set request
		reply_topic_info(*ch, chname, cl, fd, clients);
		return false;
	}

	if (!can_set_topic(*ch, chname, fd, cl, clients))
		return false;

	std::string new_topic = extract_new_topic(rest);

	apply_topic_change(*ch, cl, new_topic);
	announce_topic_change(clients, *ch, cl, (*ch).topic);

	return false;
}