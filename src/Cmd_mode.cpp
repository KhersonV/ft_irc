#include "Cmd_core.hpp"
#include "Proto.hpp"
#include "utils.hpp"
#include "State.hpp"
#include "Channel.hpp"
#include <sstream>

using namespace ftirc;

bool handle_MODE(int fd, Client &cl,
						std::map<int, Client> &clients,
						const std::string &rest)
{
	if (!cl.registered) {
		send_numeric(clients, fd, 451, cl.nick, "", "Not registered");
		return false;
	}
	if (rest.empty()) {
		send_numeric(clients, fd, 461, cl.nick, "MODE", "Not enough parameters");
		return false;
	}

	// trim leading spaces; rest2 is used to avoid modifying `rest`
	std::string rest2 = ltrim(rest);
	std::string chname = first_token(rest2);
	if (chname.empty() || chname[0] != '#') {
		send_numeric(clients, fd, 403, cl.nick, chname, "No such channel");
		return false;
	}

	std::string key = ftirc::lower_str(chname);
	std::map<std::string, Channel>::iterator it = g_state.channels.find(key);
	if (it == g_state.channels.end()) {
		send_numeric(clients, fd, 403, cl.nick, chname, "No such channel");
		return false;
	}

	Channel &ch = it->second;
	std::string tail =
		(rest2.size() > chname.size()) ? ltrim(rest2.substr(chname.size()))
									: std::string();

	if (!tail.empty()) {
		if (tail == "+t" || tail == "-t") {
			if (!is_member(ch, fd)) {
				send_numeric(clients, fd, 442, cl.nick, ch.name,
							"You're not on that channel");
				return false;
			}
			if (!is_op(ch, fd)) {
				send_numeric(clients, fd, 482, cl.nick, ch.name,
							"You're not channel operator");
				return false;
			}
			bool new_val = (tail == "+t");
			if (ch.topic_restricted != new_val) {
				ch.topic_restricted = new_val;
				std::string line = ":" + cl.nick + " MODE " + ch.name + " " + tail;
				send_to_channel(clients, ch, line, -1);
			}
			return false;
		} else {
			char bad = 0;
			for (size_t i = 0; i < tail.size(); ++i) {
				if (tail[i] != ' ' && tail[i] != '\t' && tail[i] != '+'
					&& tail[i] != '-') {
					bad = tail[i];
					break;
				}
			}
			std::string bad_s(1, bad ? bad : '?');
			send_numeric(clients, fd, 472, cl.nick, bad_s,
						"is unknown mode char to me");
			return false;
		}
	}

	std::string modes = "+";
	if (ch.topic_restricted) modes += 't';
	if (ch.invite_only)      modes += 'i';
	if (!ch.key.empty())     modes += 'k';
	if (ch.user_limit > 0)   modes += 'l';
	if (modes == "+")        modes = '+'; // preserves original behavior

	std::string modeParams;
	if (!ch.key.empty()) {
		modeParams += " *";
	}
	if (ch.user_limit > 0) {
		// C++98-compatible integer to string
		std::ostringstream ss;
		ss << ch.user_limit;
		modeParams += " " + ss.str();
	}

	std::string params = ch.name + " " + modes + modeParams;
	send_numeric(clients, fd, 324, cl.nick, params, "");
	return false;
}