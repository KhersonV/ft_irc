
#ifndef STATE_HPP
# define STATE_HPP
# include "Client.hpp"
# include <map>
# include <string>
# include "Channel.hpp"

struct			State
{
	std::string server_password;
	std::map<std::string, int> nick2fd;
	std::map<std::string, Channel> channels;
};
extern State	g_state;

#endif
