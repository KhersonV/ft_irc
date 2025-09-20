#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <ctime>
#include <list>

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

#endif