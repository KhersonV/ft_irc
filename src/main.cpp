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

int main(void) {
	signal(SIGPIPE, SIG_IGN);

	std::vector<int> clients;
	std::vector<pollfd> pfds;
	std::map<int, std::string> inbuf;
	std::map<int, std::string> outbuf;

	int port = 6667;
	int server_fd = ftirc::create_listen_socket(port);
	if (server_fd < 0)
		return 1;

	for (;;) {
		pfds.clear();

		pollfd sp;
		sp.fd = server_fd;
		sp.events = POLLIN;
		sp.revents = 0;
		pfds.push_back(sp);

		for (size_t i = 0; i < clients.size(); ++i) {
			pollfd p;
			p.fd = clients[i];
			p.events = POLLIN;
			p.revents = 0;
			if (!outbuf[clients[i]].empty()) {
				p.events |= POLLOUT;
			}
			pfds.push_back(p);
		}

		int ret = poll(&pfds[0], pfds.size(), -1);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			std::perror("poll");
			break;
		}

		if (pfds[0].revents & POLLIN) {
			for (;;) {
				int cfd = accept(server_fd, 0, 0);
				if (cfd >= 0) {
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

		for (size_t i = 1; i < pfds.size(); ++i) {
			int fd = pfds[i].fd;
			short re = pfds[i].revents;

			if (re & POLLIN) {
				char buf[4096];
				for (;;) {
					ssize_t n = recv(fd, buf, sizeof(buf), 0);
					if (n > 0) {
						inbuf[fd].append(buf, n);

						for (;;) {
							std::string line;
							if (!ftirc::cut_line(inbuf[fd], line, ftirc::debug_lf_mode())) break;
							std::cout << "LINE fd=" << fd << " : \"" << line << "\"\n";
							outbuf[fd] += "You said: " + line + "\r\n";
						}
						continue;
					}
					if (n == 0) {
						std::cout << "EOF fd=" << fd << "\n";
						ftirc::close_and_remove(fd, clients, inbuf, outbuf);
						break;
					}
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						break;
					}
					std::perror("recv");
					ftirc::close_and_remove(fd, clients, inbuf, outbuf);
					break;
				}
			}

			if (re & POLLOUT) {
				std::string &q = outbuf[fd];
				if (!q.empty()) {
					ssize_t n = send(fd, q.c_str(), q.size(), 0);
					if (n > 0) {
						q.erase(0, n);
					} else if (n < 0) {
						if (errno == EAGAIN || errno == EWOULDBLOCK) {
							// later
						} else {
							std::perror("send");
							ftirc::close_and_remove(fd, clients, inbuf, outbuf);
						}
					}
				}
			}

			if (re & (POLLERR | POLLHUP | POLLNVAL)) {
				ftirc::close_and_remove(fd, clients, inbuf, outbuf);
				continue;
			}
		}
	}

	close(server_fd);
	return 0;
}
