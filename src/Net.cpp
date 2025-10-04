#include "Cmd_core.hpp"
#include "Net.hpp"
#include "utils.hpp"
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

// outgoing packets from server (to a client)
void handle_write_ready(int fd, std::map<int, Client> &clients, std::vector<int> &fds)
{
	std::map<int, Client>::iterator it = clients.find(fd);
	if (it == clients.end()) return;
	Client &c = it->second;

	if (!c.out.empty()) {
		ssize_t n = send(fd, c.out.c_str(), c.out.size(), 0);
		if (n > 0) {
			c.out.erase(0, n);
		} else if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return;
			}
			std::perror("send");
			ftirc::close_and_remove(fd, fds, clients);
			return;
		}
	}

	if (c.closing && c.out.empty()) {
		::shutdown(fd, SHUT_RDWR);
		ftirc::close_and_remove(fd, fds, clients);
		return;
	}
}
// incoming packets to server
bool	handle_read_ready(int fd, std::map<int, Client> &clients,
		std::vector<int> &fds)
{
	char	buf[4096];
	ssize_t	n;

	std::map<int, Client>::iterator it = clients.find(fd);
	if (it == clients.end())
		return (true);
	Client &c = it->second;
	for (;;)
	{
		n = recv(fd, buf, sizeof(buf), 0);
		if (n > 0)
		{
			c.in.append(buf, n);
			for (;;)
			{
				std::string line;
				if (!ftirc::cut_line(c.in, line, ftirc::debug_lf_mode()))
					break ;
				std::cout << "LINE fd=" << fd << " : \"" << line << "\"\n";
				if (process_line(fd, line, clients))
					return (true);
			}
			continue ;
		}
		if (n == 0)
		{
			std::cout << "EOF fd=" << fd << "\n";
			ftirc::close_and_remove(fd, fds, clients);
			return (true);
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			return (false);
		}
		std::perror("recv");
		ftirc::close_and_remove(fd, fds, clients);
		return (true);
	}
}

void add_client(int cfd, const std::string &ip,
	std::vector<int>& fds, std::map<int, Client>& clients)
{
	ftirc::set_nonblocking(cfd);
	std::cout << "New client fd=" << cfd << "\n";
	fds.push_back(cfd);
	std::string cleanIp;
	if (ip.size() > 7 && ip.compare(0, 7, "::ffff:") == 0) {
		cleanIp = ip.substr(7);
	} else {
		cleanIp = ip;
	}

	clients.insert(std::make_pair(cfd, Client(cfd, cleanIp)));
}
