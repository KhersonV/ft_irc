#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>

struct Channel {
	std::string name;
	std::set<int> members;
	std::set<int> ops;
	std::string topic;
};

#endif