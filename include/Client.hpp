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
	Client():	fd(-1), pass_ok(false), registered(false) {}
	explicit	Client(int f): fd(f), pass_ok(false), registered(false) {}
};

inline std::string user_prefix(const Client &c) {
	const std::string host = "localhost";
	const std::string user = c.user.empty() ? c.nick : c.user;
	const std::string nick = c.nick.empty() ? "*" : c.nick;
	return ":" + nick + "!" + user + "@" + host;
}

#endif