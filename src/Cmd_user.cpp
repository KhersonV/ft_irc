#include "Cmd_core.hpp"
#include "Proto.hpp"
#include "Irc_codes.hpp"

using namespace ft_codes;

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

bool parse_user_params(const std::string &rest, std::string &username, std::string &realname, int fd, const Client &cl, std::map<int, Client> &clients)
{
	if (rest.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "USER",
					"syntax: USER <username> 0 * :<realname>");
		return false;
	}

	username = rest;
	std::string::size_type sp1 = username.find(' ');
	if (sp1 != std::string::npos)
		username.erase(sp1);

	if (username.empty()) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "USER",
					"Need username: USER <username> 0 * :<realname>");
		return false;
	}

	std::string::size_type pos_trailing = rest.find(" :");
	if (pos_trailing == std::string::npos) {
		send_numeric(clients, fd, NEED_MORE_PARAMS, cl.nick, "USER",
					"Need semicolon before real name: USER <username> 0 * :<realname>");
		return false;
	}
	realname = rest.substr(pos_trailing + 2);

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
