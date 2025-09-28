#include "utils.hpp"
#include "Client.hpp"
#include "State.hpp"


#include <sys/socket.h>
#include <netinet/in.h>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cstdlib>

namespace ftirc {

bool debug_lf_mode() {
	const char* p = std::getenv("FTIRC_DEBUG_LF");
	return p && *p;
}

std::string lower_str(const std::string& nickname) {
	std::string result(nickname);
	for (size_t i = 0; i < result.size(); ++i) {
		result[i] = std::tolower(static_cast<unsigned char>(result[i]));
	}
	return result;
}

bool cut_line(std::string &ib, std::string &line, bool debugLF)
{
	if (debugLF) {
		std::string::size_type pos = ib.find('\n');
		if (pos == std::string::npos) return false;
		line = ib.substr(0, pos);
		ib.erase(0, pos + 1);
		if (!line.empty() && line[line.size()-1] == '\r')
			line.erase(line.size()-1);
		return true;
	} else {
		std::string::size_type pos = ib.find("\r\n");
		if (pos == std::string::npos) return false;
		line = ib.substr(0, pos);
		ib.erase(0, pos + 2);
		return true;
	}
}

// int set_nonblocking(int fd) {
// 	int flags = fcntl(fd, F_GETFL, 0);
// 	if (flags == -1) {
// 		return -1;
// 	}
// 	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
// 		return -1;
// 	}
// 	return 0;
// }

int set_nonblocking(int fd) {
	return (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) ? -1 : 0;
}

int create_listen_socket(int port) {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		std::perror("socket");
		return -1;
	}
	int yes = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		std::perror("setsockopt SO_REUSEADDR");
		close(fd);
		return -1;
	}
	if (set_nonblocking(fd) < 0) {
		std::perror("fcntl O_NONBLOCK");
		close(fd);
		return -1;
	}

	sockaddr_in address;
	std::memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(port);

	if (bind(fd, (sockaddr*)&address, sizeof(address)) < 0) {
		std::perror("bind");
		close(fd);
		return -1;
	}
	int backlog = 128;
	if (listen(fd, backlog) < 0) {
		std::perror("listen");
		close(fd);
		return -1;
	}
	std::cout << "Listening...\n";
	return fd;
}

void close_and_remove(int fd,
					  std::vector<int>& fds,
					  std::map<int, Client>& clients)
{
	std::map<int, Client>::iterator it = clients.find(fd);
	if (it != clients.end()) {
		const std::string& nick = it->second.nick;
		if (!nick.empty()) {
			g_state.nick2fd.erase(lower_str(nick));
			g_state.reservednick2fd.erase(lower_str(nick));
		}
	}
	close(fd);
	fds.erase(std::remove(fds.begin(), fds.end(), fd), fds.end());
	clients.erase(fd);
	std::cout << "Close fd=" << fd << "\n";
}

 std::string to_upper(std::string s) {
	for (size_t i = 0; i < s.size(); ++i)
		s[i] = std::toupper(static_cast<unsigned char>(s[i]));
	return s;
}

std::string ltrim(const std::string &s)
{
	std::string::size_type i = 0;
	while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
		++i;
	return (s.substr(i));
}

std::string first_token(const std::string &s)
{
	std::string::size_type ws = s.find(' ');
	return (ws == std::string::npos) ? s : s.substr(0, ws);
}

}
