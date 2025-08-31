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

	//modes
	bool	topic_restricted; // +t
	bool	invite_only; // +i
	std::string	key; // +k (empty - no key)
	int			user_limit; // +l (0 - no lmit)
	std::set<int>	invided; // +i (invited users)

	Channel()
	: topic_set_at(0),
	topic_restricted(false),
	invite_only(false),
	user_limit(0)
	{}
};

#endif