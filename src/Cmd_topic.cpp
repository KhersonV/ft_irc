#include "Cmd_core.hpp"
#include "Channel.hpp"
#include "Proto.hpp"
#include "Irc_codes.hpp"
#include "utils.hpp"
#include "State.hpp"
#include <sstream>

using namespace ft_codes;
using namespace ftirc;

bool handle_TOPIC(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest)
{
	if (!cl.registered) {
		send_numeric(clients, fd, NOT_REGISTERED,
					cl.nick.empty() ? "*" : cl.nick, "",
					"You have not registered");
		return false;
	}
	if (rest.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "TOPIC",
					"Not enough parameters");
		return false;
	}

	std::string chname = first_token(rest);
	if (chname.empty() || chname[0] != '#') {
		send_numeric(clients, fd, NO_SUCH_CHANNEL, cl.nick, chname,
					"No such channel");
		return false;
	}

	std::string key = lower_str(chname);
	std::map<std::string, Channel>::iterator itCh = g_state.channels.find(key);
	if (itCh == g_state.channels.end()) {
		send_numeric(clients, fd, NO_SUCH_CHANNEL, cl.nick, chname,
					"No such channel");
		return false;
	}

	Channel &ch = itCh->second;
	if (ch.members.find(fd) == ch.members.end()) {
		send_numeric(clients, fd, NOT_ON_CHANNEL, cl.nick, chname,
					"You're not on that channel");
		return false;
	}

	std::string::size_type pos_trailing = rest.find(" :");
	if (pos_trailing == std::string::npos) {
		// Query current topic
		if (ch.topic.empty()) {
			send_numeric(clients, fd, NO_TOPIC, cl.nick, chname,
						"No topic is set");
		} else {
			send_numeric(clients, fd, TOPIC, cl.nick, chname, ch.topic);

			std::ostringstream p;
			p << chname << " "
			<< (ch.topic_set_by.empty() ? "*" : ch.topic_set_by) << " "
			<< (ch.topic_set_at ? ch.topic_set_at : 0);
			send_numeric(clients, fd, TOPIC_WHO_TIME, cl.nick, p.str(), "");
		}
		return false;
	}

	// Set new topic
	std::string new_topic = rest.substr(pos_trailing + 2);
	if (ch.topic_restricted && ch.ops.find(fd) == ch.ops.end()) {
		send_numeric(clients, fd, CHAN_OP_PRIVS_NEEDED, cl.nick, chname,
					"You're not channel operator");
		return false;
	}

	ch.topic = new_topic;
	ch.topic_set_by = cl.nick;
	ch.topic_set_at = std::time(NULL);

	std::string line = ":" + cl.nick + " TOPIC " + ch.name + " :" + new_topic;
	send_to_channel(clients, ch, line, -1);
	return false;
}