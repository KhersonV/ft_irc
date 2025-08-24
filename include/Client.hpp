#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <ctime>

struct Client {
	int		fd;
	std::string	in;
	std::string	out;
	std::string	nick;
	std::string user;
	std::string	realname;
	bool		pass_ok;
	bool		registered;
	Client():	fd(-1), pass_ok(false), registered(false) {}
	explicit	Client(int f): fd(f), pass_ok(false), registered(false) {}
};

#endif