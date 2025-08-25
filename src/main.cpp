#include <sys/socket.h>
#include <netinet/in.h>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdio>
#include <poll.h>
#include <map>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <signal.h>

#include "utils.hpp"
#include "Client.hpp"
#include "Cmd_core.hpp"
#include "State.hpp"

static void handle_write_ready(int fd,
							   std::map<int, Client>& clients,
							   std::vector<int>& fds
							)
{
	std::map<int, Client>::iterator it = clients.find(fd);
	if (it == clients.end()) return;
	Client &c = it->second;
	if (c.out.empty()) return;

	ssize_t n = send(fd, c.out.c_str(), c.out.size(), 0);
	if (n > 0) {
		c.out.erase(0, n);
		return;
	}
	if (n < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) return;
		std::perror("send");
		ftirc::close_and_remove(fd, fds, clients);
	}
}

static bool handle_read_ready(int fd,
							  std::map<int, Client>& clients,
							  std::vector<int>& fds)
{
	std::map<int, Client>::iterator it = clients.find(fd);
		if (it == clients.end()) return true;
	Client &c = it->second;
	char buf[4096];

	for (;;) {
		ssize_t n = recv(fd, buf, sizeof(buf), 0);
		if (n > 0) {
			c.in.append(buf, n);
			for (;;) {
				std::string line;
				if (!ftirc::cut_line(c.in, line, ftirc::debug_lf_mode())) break;
				std::cout << "LINE fd=" << fd << " : \"" << line << "\"\n";
				if (process_line(fd, line, clients, fds))
					return true;
			}
			continue;
		}
		if (n == 0) {
			std::cout << "EOF fd=" << fd << "\n";
			ftirc::close_and_remove(fd, fds, clients);
			return true;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return false;
		}
		std::perror("recv");
		ftirc::close_and_remove(fd, fds, clients);
		return true;
	}
}

int main(void)
{
	signal(SIGPIPE, SIG_IGN);

	std::vector<pollfd> pfds;
	std::vector<int> fds;
	std::map<int, Client> clients;

	int port = 6667;
	int server_fd = ftirc::create_listen_socket(port);
	if (server_fd < 0)
		return 1;

	for (;;)
	{
		pfds.clear();

		pollfd sp;
		sp.fd = server_fd;
		sp.events = POLLIN;
		sp.revents = 0;
		pfds.push_back(sp);

		for (size_t i = 0; i < fds.size(); ++i)
		{
			int cfd = fds[i];
			pollfd p;
			p.fd = cfd;
			p.events = POLLIN;
			p.revents = 0;

			std::map<int, Client>::iterator it = clients.find(cfd);

			if (it != clients.end() && !it->second.out.empty())
				p.events |= POLLOUT;
			pfds.push_back(p);
		}

		int ret = poll(&pfds[0], pfds.size(), -1);
		if (ret < 0)
		{
			if (errno == EINTR)
				continue;
			std::perror("poll");
			break;
		}

		if (pfds[0].revents & POLLIN)
		{
			for (;;)
			{
				int cfd = accept(server_fd, 0, 0);
				if (cfd >= 0)
				{
					ftirc::set_nonblocking(cfd);
					std::cout << "New client fd=" << cfd << "\n";
					fds.push_back(cfd);
					clients.insert(std::make_pair(cfd, Client(cfd)));
					continue;
				}
				if (errno == EAGAIN || errno == EWOULDBLOCK)
					break;
				std::perror("accept");
				break;
			}
		}

		for (size_t i = 1; i < pfds.size(); ++i)
		{
			int fd = pfds[i].fd;
			short re = pfds[i].revents;
			bool closed = false;

			if (re & POLLIN)
			{
				closed = handle_read_ready(fd, clients, fds);
			}
			if (closed) continue;

			if (re & POLLOUT)
			{
				handle_write_ready(fd, clients, fds);
			}
			if (re & (POLLERR | POLLHUP | POLLNVAL))
			{
				ftirc::close_and_remove(fd, fds, clients);
				continue;
			}
		}
	}

	close(server_fd);
	return 0;
}
