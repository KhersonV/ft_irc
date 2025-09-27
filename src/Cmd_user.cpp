#include "Cmd_core.hpp"
#include "Proto.hpp"
#include "Irc_codes.hpp"

using namespace ft_codes;
using namespace	ftirc;

namespace
{
bool is_register_possible(Client &cl, int fd, std::map<int, Client> &clients)
{
	if (cl.registered) {
		send_numeric(clients, fd, ALREADY_REGISTERED, cl.nick, "",
					"You may not re-register");
		return false;
	}
	return true;
}

bool parse_user_params(const std::string &rest,
						std::string &username,
						std::string &realname,
						int fd, const Client &cl,
						std::map<int, Client> &clients)
{
	if (rest.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "USER",
					 "syntax: USER <username> 0 * :<realname>");
		return false;
	}

	std::string s = ltrim(rest);

	username = first_token(s);
	if (username.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "USER",
					 "Need username: USER <username> 0 * :<realname>");
		return false;
	}
	s = ltrim(s.substr(username.size()));

	std::string mode = first_token(s);
	if (mode.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "USER",
					 "Need mode field: USER <username> 0 * :<realname>");
		return false;
	}
	s = ltrim(s.substr(mode.size()));

	std::string unused = first_token(s);
	if (unused.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "USER",
					 "Need unused field: USER <username> 0 * :<realname>");
		return false;
	}
	s = ltrim(s.substr(unused.size()));

	if (s.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "USER",
					 "Need real name: USER <username> 0 * :<realname>");
		return false;
	}

	if (s[0] == ':') {
		realname = s.substr(1);
	} else {
		std::string::size_type pos = s.find(" :");
		if (pos == std::string::npos) {
			send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "USER",
						 "Need semicolon before real name: USER <username> 0 * :<realname>");
			return false;
		}
		realname = s.substr(pos + 2);
	}

	if (realname.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "USER",
					 "Real name cannot be empty");
		return false;
	}
	return true;
}

inline void apply_user_info(Client &cl, const std::string &username, const std::string &realname)
{
	cl.user = username;
	cl.realname = realname;
}
} // namespace

bool handle_USER(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest)
{
	if (!is_register_possible(cl, fd, clients))
		return false;

	std::string username, realname;
	if (!parse_user_params(rest, username, realname, fd, cl, clients))
		return false;

	apply_user_info(cl, username, realname);
	finish_register(clients, fd);
	return false;
}
