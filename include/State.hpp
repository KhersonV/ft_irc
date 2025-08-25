
#ifndef STATE_HPP
# define STATE_HPP
# include "Client.hpp"
# include <map>
# include <string>

struct			State
{
	std::string server_password;
	std::map<std::string, int> nick2fd;
};
extern State	g_state;

#endif
