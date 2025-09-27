#include "Cmd_core.hpp"

using namespace ftirc;
using namespace ft_codes;

namespace
{
static std::string user_prefix(const Client &c) {
	const std::string host = "localhost";
	const std::string user = c.user.empty() ? c.nick : c.user;
	const std::string nick = c.nick.empty() ? "*" : c.nick;
	return ":" + nick + "!" + user + "@" + host;
}

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

bool parse_privmsg(const std::string &rest, std::string &target, std::string &text, const Client &cl, int fd, std::map<int, Client> &clients, const std::string &cmd)
{
	if (rest.empty()) {
		send_numeric(clients, fd, NO_RECIPIENT, cl.nick, cmd,
					"No recipient given (" + cmd + ")");
		return false;
	}

	target = rest;
	std::string::size_type sp = target.find(' ');
	if (sp != std::string::npos) target.erase(sp);

	if (target.empty()) {
		send_numeric(clients, fd, NO_RECIPIENT, cl.nick, cmd,
					"No recipient given (" + cmd + ")");
		return false;
	}

	std::string::size_type pos_trailing = rest.find(" :");
	if (pos_trailing == std::string::npos) {
		send_numeric(clients, fd, NO_TEXT_TO_SEND, cl.nick, "", "No text to send");
		return false;
	}

	text = rest.substr(pos_trailing + 2);
	return true;
}

bool send_privmsg_to_channel(std::map<int, Client> &clients, int fd, const Client &cl, const std::string &target_chan, const std::string &text, const std::string &cmd)
{
	Channel *ch = 0;
	if (!find_channel(target_chan, ch, cl, fd, clients)) {
		return (false);
	}

	if (!is_member(*ch, fd)) {
		send_numeric(clients, fd, CANNOT_SEND_TO_CHAN, cl.nick, target_chan,
					"cannot send to channel");
		return false;
	}

	const std::string line =
		user_prefix(cl) + " " + cmd + " " + target_chan + " :" + text;
	send_to_channel(clients, *ch, line, fd);
	return true;
}

bool find_target_fd_by_nick(const std::string &nick, int &out_fd, const Client &cl, int fd, std::map<int, Client> &clients)
{
	std::map<std::string,int>::iterator it = g_state.nick2fd.find(lower_str(nick));
	if (it == g_state.nick2fd.end()) {
		send_numeric(clients, fd, NO_SUCH_NICK, cl.nick, nick, "No such nick");
		return false;
	}
	out_fd = it->second;
	return true;
}

inline void send_privmsg_to_user(std::map<int, Client> &clients, int rfd, const Client &cl, const std::string &target_nick, const std::string &text, const std::string &cmd)
{
	const std::string line =
		user_prefix(cl) + " " + cmd + " " + target_nick + " :" + text;
	enqueue_line(clients, rfd, line);
}

} // anonymous namespace

bool handle_PRIVMSG(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest, const std::string &cmd)
{
	if (!is_registered(cl, fd, clients))
		return false;

	std::string target, text;
	if (!parse_privmsg(rest, target, text, cl, fd, clients, cmd))
		return false;

	if (!target.empty() && target[0] == '#') {
		send_privmsg_to_channel(clients, fd, cl, target, text, cmd);
		return false;
	}

	int rfd = -1;
	if (!find_target_fd_by_nick(target, rfd, cl, fd, clients))
		return false;

	send_privmsg_to_user(clients, rfd, cl, target, text, cmd);
	return false;
}
