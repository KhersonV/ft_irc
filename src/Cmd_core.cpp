#include "Cmd_core.hpp"
#include "utils.hpp"
#include "Proto.hpp"
#include "State.hpp"

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
