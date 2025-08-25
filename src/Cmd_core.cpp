#include "Cmd_core.hpp"
#include "utils.hpp"
#include "Proto.hpp"
#include "State.hpp"

static std::string lower_str(const std::string& nickname) {
	std::string result(nickname);
	for (size_t i = 0; i < result.size(); ++i) {
		result[i] = std::tolower(static_cast<unsigned char>(result[i]));
	}
	return result;
}

static bool is_valid_nick(const std::string& nickname) {
	if (nickname.empty() || nickname.size() > 30) {
		return false;
	}
	for (size_t i = 0; i < nickname.size(); ++i) {
		unsigned char c = nickname[i];
		if (std::isalnum(c)) {
			continue;
		}
		switch (c) {
			case '-': case '_': case '[': case ']': case '\\':
			case '`': case '^': case '{': case '}':
				continue;
			default: return false;
		}
	}
	return true;
}

static void finish_register(std::map<int, Client>& clients, int fd) {
	Client &c = clients[fd];

	if (c.registered) return ;

	if (c.nick.empty() || c.user.empty()) return ;

	if (c.nick.empty() || c.user.empty()) return ;

	c.registered = true;

	send_numeric(clients, fd, 001, c.nick, "", "Welcome to ft_irc " + c.nick);
	send_numeric(clients, fd, 002, c.nick, "", "Your host is ft_irc");
}

std::string first_token(const std::string& s)
{
	std::string::size_type ws = s.find(' ');
	return (ws == std::string::npos) ? s : s.substr(0, ws);
}

bool process_line(int fd,
						const std::string& line,
						std::map<int, Client>& clients,
						 std::vector<int>& fds)
{
	std::string s = line;
	if (!s.empty() && s[0] == ':')
	{
		size_t sp = s.find(' ');
		if (sp != std::string::npos) s.erase(0, sp + 1);
		else s.clear();
	}
	size_t sp = s.find(' ');
	std::string cmd  = (sp == std::string::npos) ? s : s.substr(0, sp);
	std::string rest = (sp == std::string::npos) ? "" : s.substr(sp + 1);
	cmd = ftirc::to_upper(cmd);

	if (cmd.empty())
		return false;
	Client &cl = clients[fd];
	if (cmd == "PASS")
	{
		if (cl.registered)
		{
			send_numeric(clients, fd, 462, cl.nick, "", "You may not reregister");
			return false;
		}
		if (rest.empty())
		{
			send_numeric(clients, fd, 461, cl.nick, "PASS", "Not enough parameters");
			return false;
		}
		std::string pass = rest;
		if (!pass.empty() && pass[0] == ':')
			pass.erase(0, 1);
		if (!g_state.server_password.empty() && pass != g_state.server_password)
		{
			send_numeric(clients, fd, 464, cl.nick, "", "Password incorrect");
			return false;
		}
		cl.pass_ok = true;
		finish_register(clients, fd);
		return false;
	}
	if (cmd == "NICK")
	{
		if (rest.empty())
		{
			send_numeric(clients, fd, 431, cl.nick, "", "No nickname given");
			return false;
		}
		std::string nick = rest;
		if (!nick.empty() && nick[0] == ':')
			nick.erase(0, 1);
		if (!is_valid_nick(nick)) {
			send_numeric(clients, fd, 432, cl.nick, nick, "Nick contains invalid characters");
			return false;
		}

		std::string key = lower_str(nick);
		if (g_state.nick2fd.find(key) != g_state.nick2fd.end()
			&& g_state.nick2fd[key] != fd)
		{
			send_numeric(clients, fd, 433, cl.nick, nick, "Nickname is already in use");
			return false;
		}
		if (!cl.nick.empty())
		{
			std::map<std::string,int>::iterator old = g_state.nick2fd.find(lower_str(cl.nick));
			if (old != g_state.nick2fd.end() && old->second == fd)
				g_state.nick2fd.erase(old);
		}
		cl.nick = nick;
		g_state.nick2fd[key] = fd;
		finish_register(clients, fd);
		return false;
	}
	if (cmd == "USER") {
		if (cl.registered) {
			send_numeric(clients, fd, 462, cl.nick, "", "You may not reregister");
			return false;
		}

		if (rest.empty()) {
			send_numeric(clients, fd, 461, cl.nick, "USER", "Not enough parameters");
			return false;
		}

		std::string username = rest;
		size_t sp1 = username.find(' ');
		if (sp1 != std::string::npos) {
			username.erase(sp1);
		}

		std::string realname;
		size_t pos_trailing = rest.find(" :");
		if (pos_trailing != std::string::npos) {
			realname = rest.substr(pos_trailing + 2);
		} else {
			send_numeric(clients, fd, 461, cl.nick, "USER", "Not enough parameters");
			return false;
		}

		if(username.empty()) {
			send_numeric(clients, fd, 461, cl.nick, "USER", "Not enough parameters");
			return false;
		}

		cl.user = username;
		cl.realname = realname;

		finish_register(clients, fd);
		return false;

	}
	if (cmd == "PING")
	{
		if (!rest.empty() && rest[0] == ':')
			rest.erase(0, 1);
		if (rest.empty())
			rest = "ft_irc";
		enqueue_line(clients, fd, "PONG :" + rest);
		return false;
	}
	if (cmd == "QUIT")
	{
		ftirc::close_and_remove(fd, fds, clients);
		return true;
	}
	enqueue_line(clients, fd, "You said: " + line);
	return false;
}
