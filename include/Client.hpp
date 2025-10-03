#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <ctime>
#include <list>
#include "Channel.hpp"

struct Client {
	int		fd;
	std::string	in;
	std::string	out;
	std::string	nick;
	std::string user;
	std::string	realname;
	std::list<Channel*> channels;
	bool		pass_ok;
	bool		registered;
	bool		closing;

	std::string ip;

	Client():	fd(-1), pass_ok(false), registered(false), ip("127.0.0.1") {}
	explicit	Client(int f): fd(f), pass_ok(false), registered(false), ip("127.0.0.1") {}
	explicit	Client(int f, const std::string& i): fd(f), pass_ok(false), registered(false), ip(i) {}
};

// inline std::string user_prefix(const Client &c) {
// 	const std::string host = (c.ip.empty() || c.ip == "127.0.0.1") ? "localhost" : c.ip;
// 	const std::string user = c.user.empty() ? c.nick : c.user;
// 	const std::string nick = c.nick.empty() ? "*" : c.nick;
// 	return ":" + nick + "!" + user + "@" + host;
// }

// static std::string build_prefix_for_old_nick(const Client &cl, const std::string &old_nick)
// {
// 	const std::string host = "localhost";
// 	const std::string user = cl.user.empty() ? old_nick : cl.user;
// 	const std::string nick = old_nick.empty() ? "*" : old_nick;
// 	return ":" + nick + "!" + user + "@" + host;
// }

inline std::string user_prefix(const Client &c) {
	const std::string host = (c.ip.empty() || c.ip == "127.0.0.1" || c.ip == "::1")
		? "localhost" : c.ip;
	const std::string nick = c.nick.empty() ? "*" : c.nick;

	std::string p = ":" + nick;
	if (!c.user.empty()) p += "!" + c.user;      // omit if user not set yet
	p += "@" + host;
	return p;
}

inline std::string user_prefix_old_nick(const Client &c, const std::string &old_nick) {
	const std::string host = (c.ip.empty() || c.ip == "127.0.0.1" || c.ip == "::1")
		? "localhost" : c.ip;
	const std::string nick = old_nick.empty() ? "*" : old_nick;

	std::string p = ":" + nick;
	if (!c.user.empty()) p += "!" + c.user;      // don't substitute old_nick as user
	p += "@" + host;
	return p;
}

#endif