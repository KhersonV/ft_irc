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
#include <sstream>

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
	// 1) Try IPv6 listener (prefer dual-stack if OS allows)
	int fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (fd >= 0) {
		int yes = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
			std::perror("setsockopt SO_REUSEADDR (v6)");
			close(fd);
			return -1;
		}

		// Try to allow dual-stack (IPv4 via v4-mapped). Some OSes ignore this.
		int v6only = 0;
		(void)setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));

		if (set_nonblocking(fd) < 0) {
			std::perror("fcntl O_NONBLOCK (v6)");
			close(fd);
			return -1;
		}

		struct sockaddr_in6 addr6;
		std::memset(&addr6, 0, sizeof(addr6));
		addr6.sin6_family = AF_INET6;
		addr6.sin6_addr   = in6addr_any;           // ::
		addr6.sin6_port   = htons(port);

		if (bind(fd, (struct sockaddr*)&addr6, sizeof(addr6)) == 0) {
			int backlog = 128;
			if (listen(fd, backlog) == 0) {
				std::cout << "Listening on [::]:" << port << " (IPv6";
				std::cout << ", dual-stack " << (v6only ? "off?" : "attempted") << ")\n";
				return fd;
			}
			std::perror("listen (v6)");
		} else {
			std::perror("bind (v6)");
		}
		close(fd);
		// fall through to IPv4 fallback
	}

	// 2) Fallback: IPv4-only listener
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		std::perror("socket (v4)");
		return -1;
	}
	int yes = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		std::perror("setsockopt SO_REUSEADDR (v4)");
		close(fd);
		return -1;
	}
	if (set_nonblocking(fd) < 0) {
		std::perror("fcntl O_NONBLOCK (v4)");
		close(fd);
		return -1;
	}

	struct sockaddr_in addr4;
	std::memset(&addr4, 0, sizeof(addr4));
	addr4.sin_family      = AF_INET;
	addr4.sin_addr.s_addr = htonl(INADDR_ANY);     // 0.0.0.0
	addr4.sin_port        = htons(port);

	if (bind(fd, (struct sockaddr*)&addr4, sizeof(addr4)) < 0) {
		std::perror("bind (v4)");
		close(fd);
		return -1;
	}
	int backlog = 128;
	if (listen(fd, backlog) < 0) {
		std::perror("listen (v4)");
		close(fd);
		return -1;
	}
	std::cout << "Listening on 0.0.0.0:" << port << " (IPv4 only)\n";
	return fd;
}

// old iv4 only version
// int create_listen_socket(int port) {
// 	int fd = socket(AF_INET, SOCK_STREAM, 0);
// 	if (fd < 0) {
// 		std::perror("socket");
// 		return -1;
// 	}
// 	int yes = 1;
// 	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
// 		std::perror("setsockopt SO_REUSEADDR");
// 		close(fd);
// 		return -1;
// 	}
// 	if (set_nonblocking(fd) < 0) {
// 		std::perror("fcntl O_NONBLOCK");
// 		close(fd);
// 		return -1;
// 	}

// 	sockaddr_in address;
// 	std::memset(&address, 0, sizeof(address));
// 	address.sin_family = AF_INET;
// 	address.sin_addr.s_addr = htonl(INADDR_ANY);
// 	address.sin_port = htons(port);

// 	if (bind(fd, (sockaddr*)&address, sizeof(address)) < 0) {
// 		std::perror("bind");
// 		close(fd);
// 		return -1;
// 	}
// 	int backlog = 128;
// 	if (listen(fd, backlog) < 0) {
// 		std::perror("listen");
// 		close(fd);
// 		return -1;
// 	}
// 	std::cout << "Listening...\n";
// 	return fd;
// }

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

std::vector<std::string> split(const std::string& s, char delimiter) {
	std::vector<std::string> tokens;
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delimiter)) {
		tokens.push_back(item);
	}
	return tokens;
}

}
