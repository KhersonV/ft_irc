#include "Cmd_core.hpp"

using namespace ftirc;

namespace
{
	void ensure_operator_if_needed(Channel &ch)
	{
		if (!ch.ops.empty())
			return;
		if (ch.members.empty())
			return;
		ch.ops.insert(*ch.members.begin());
	}


	std::string parse_quit_reason(const std::string &rest)
	{
		if (rest.empty()) return std::string();
		if (rest[0] == ':') return rest.substr(1);

		std::string::size_type pos = rest.find(" :");
		if (pos != std::string::npos) return rest.substr(pos + 2);
		return std::string();
	}

	inline const std::string display_nick(const std::string &nick) {
		return nick.empty() ? "*" : nick;
	}

	void	leave_all_channels(std::map<int, Client> &clients,
			const std::string &nick, int fd, const std::string &reason)
	{
		std::vector<std::string> to_erase;
		for (std::map<std::string,
			Channel>::iterator it = g_state.channels.begin(); it != g_state.channels.end(); ++it)
		{
			Channel &ch = it->second;
			ch.invited.erase(fd);
			if (ch.members.find(fd) != ch.members.end())
			{
				std::string line = ":" + nick + " QUIT :"
					+ (reason.empty() ? "Quit" : reason);
				send_to_channel(clients, ch, line, fd);
				remove_member_from_channel(ch, fd);
				if (ch.members.empty())
					to_erase.push_back(it->first);
				else
					ensure_operator_if_needed(ch);
			}
		}
		for (size_t i = 0; i < to_erase.size(); ++i)
		{
			g_state.channels.erase(to_erase[i]);
		}
	}

} // anonymous namespace

bool handle_QUIT(int fd, Client &cl,  std::map<int, Client> &clients, std::vector<int> &fds, const std::string &rest)
{
	const std::string reason = parse_quit_reason(rest);

	leave_all_channels(clients, display_nick(cl.nick), fd, reason);
	close_and_remove(fd, fds, clients);
	return true;
}

