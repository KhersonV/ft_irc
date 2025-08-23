#include "utils.hpp"

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

int set_nonblocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		return -1;
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		return -1;
	}
	return 0;
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
					  std::vector<int>& clients,
					  std::map<int, std::string>& inbuf,
					  std::map<int, std::string>& outbuf)
{
	close(fd);
	clients.erase(std::remove(clients.begin(), clients.end(), fd), clients.end());
	inbuf.erase(fd);
	outbuf.erase(fd);
	std::cout << "Close fd=" << fd << "\n";
}

}