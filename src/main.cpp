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
#include "Net.hpp"

int main(int argc, char** argv)
{
	signal(SIGPIPE, SIG_IGN);

	std::vector<pollfd> pfds;
	std::vector<int> fds;
	std::map<int, Client> clients;

	int port = 6667;

	if (argc >= 2)
		port = std::atoi(argv[1]);
	if (argc >= 3)
		g_state.server_password = argv[2];
	else g_state.server_password = "secret";

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
					add_client(cfd, fds, clients);
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
