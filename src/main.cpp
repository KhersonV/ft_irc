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

static void handle_write_ready(int fd,
							   std::map<int,std::string>& outbuf,
							   std::vector<int>& clients,
							   std::map<int,std::string>& inbuf)
{
	std::map<int,std::string>::iterator it = outbuf.find(fd);
	if (it == outbuf.end()) return;
	std::string &q = it->second;
	if (q.empty()) return;

	ssize_t n = send(fd, q.c_str(), q.size(), 0);
	if (n > 0) {
		q.erase(0, n);
		return;
	}
	if (n < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) return;
		std::perror("send");
		ftirc::close_and_remove(fd, clients, inbuf, outbuf);
	}
}

static bool process_line(int fd, const std::string& line,
						 std::map<int,std::string>& outbuf,
						 std::vector<int>& clients,
						 std::map<int,std::string>& inbuf)
{
	std::string s = line;
	if (!s.empty() && s[0] == ':') {
		size_t sp = s.find(' ');
		if (sp != std::string::npos) s.erase(0, sp + 1);
		else s.clear();
	}
	size_t sp = s.find(' ');
	std::string cmd  = (sp == std::string::npos) ? s : s.substr(0, sp);
	std::string rest = (sp == std::string::npos) ? "" : s.substr(sp + 1);
	cmd = ftirc::to_upper(cmd);

	if (cmd == "PING") {
		if (!rest.empty() && rest[0] == ':') rest.erase(0, 1);
		if (rest.empty()) rest = "ft_irc";
		outbuf[fd] += "PONG :" + rest + "\r\n";
		return false;
	}
	if (cmd == "QUIT") {
		ftirc::close_and_remove(fd, clients, inbuf, outbuf);
		return true;
	}
	outbuf[fd] += "You said: " + line + "\r\n";
	return false;
}

static bool handle_read_ready(int fd,
							  std::map<int,std::string>& inbuf,
							  std::map<int,std::string>& outbuf,
							  std::vector<int>& clients)
{
	char buf[4096];
	for (;;) {
		ssize_t n = recv(fd, buf, sizeof(buf), 0);
		if (n > 0) {
			inbuf[fd].append(buf, n);

			for (;;) {
				std::string line;
				if (!ftirc::cut_line(inbuf[fd], line, ftirc::debug_lf_mode())) break;
				std::cout << "LINE fd=" << fd << " : \"" << line << "\"\n";
				if (process_line(fd, line, outbuf, clients, inbuf))
					return true;
			}
			continue;
		}
		if (n == 0) {
			std::cout << "EOF fd=" << fd << "\n";
			ftirc::close_and_remove(fd, clients, inbuf, outbuf);
			return true;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return false;
		}
		std::perror("recv");
		ftirc::close_and_remove(fd, clients, inbuf, outbuf);
		return true;
	}
}

int main(void)
{
	signal(SIGPIPE, SIG_IGN);

	std::vector<int> clients;
	std::vector<pollfd> pfds;
	std::map<int, std::string> inbuf;
	std::map<int, std::string> outbuf;

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

		for (size_t i = 0; i < clients.size(); ++i)
		{
			pollfd p;
			p.fd = clients[i];
			p.events = POLLIN;
			p.revents = 0;
			if (!outbuf[clients[i]].empty())
			{
				p.events |= POLLOUT;
			}
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
					clients.push_back(cfd);
					inbuf[cfd] = "";
					outbuf[cfd] = "";
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

			if (re & (POLLERR | POLLHUP | POLLNVAL))
			{
				ftirc::close_and_remove(fd, clients, inbuf, outbuf);
				continue;
			}

			if (re & POLLIN)
			{
				closed = handle_read_ready(fd, inbuf, outbuf, clients);
			}
			if (closed) continue;  

			if (re & POLLOUT)
			{
				handle_write_ready(fd, outbuf, clients, inbuf);
			}
		}
	}

	close(server_fd);
	return 0;
}
