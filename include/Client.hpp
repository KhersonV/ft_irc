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

	std::string ip;

	Client():	fd(-1), pass_ok(false), registered(false), ip("127.0.0.1") {}
	explicit	Client(int f): fd(f), pass_ok(false), registered(false), ip("127.0.0.1") {}
	explicit	Client(int f, const std::string& i): fd(f), pass_ok(false), registered(false), ip(i) {}
};

inline std::string user_prefix(const Client &c) {
	const std::string host = (c.ip.empty() || c.ip == "127.0.0.1") ? "localhost" : c.ip;
	const std::string user = c.user.empty() ? c.nick : c.user;
	const std::string nick = c.nick.empty() ? "*" : c.nick;
	return ":" + nick + "!" + user + "@" + host;
}

#endif