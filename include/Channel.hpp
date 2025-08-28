#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include <set>
# include <string>
# include <ctime>   

struct		Channel
{
	std::string name;
	std::set<int> members;
	std::set<int> ops;
	std::string topic;
	std::string topic_set_by;
	std::time_t topic_set_at;
	bool	topic_restricted;

	Channel() : topic_set_at(0), topic_restricted(false) {}
};

#endif