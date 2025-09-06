#include "Cmd_core.hpp"
#include "Proto.hpp"
#include "State.hpp"

bool handle_PASS(int fd, Client &cl, std::map<int, Client> &clients, const std::string &rest)
{
	if (cl.registered) {
		send_numeric(clients, fd, 462, cl.nick, "", "You may not reregister");
		return false;
	}
	if (rest.empty()) {
		send_numeric(clients, fd, 461, cl.nick, "PASS", "Not enough parameters");
		return false;
	}

	std::string pass = rest;
	std::string::size_type ws = pass.find(' ');
	if (ws != std::string::npos) pass.erase(ws);
	if (!pass.empty() && pass[0] == ':') pass.erase(0, 1);

	if (!g_state.server_password.empty() && pass != g_state.server_password) {
		send_numeric(clients, fd, 464, cl.nick, "", "Password incorrect");
		return false;
	}

	cl.pass_ok = true;
	finish_register(clients, fd);
	return false;
}