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

static bool debug_lf_mode() {
	const char* p = std::getenv("FTIRC_DEBUG_LF");
	return p && *p;
}

static bool cut_line(std::string &ib, std::string &line, bool debugLF)
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


// фунция перевода fd в Non-block мод (чтобы не вис при accept(), send(), и тд)
int set_nonblocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		return -1;
	}
	if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
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
		std::perror("setsockupt SO_REUSEADDR");
		close(fd);
		return -1;
	}

	if (set_nonblocking(fd) < 0) {
		std::perror("fcntl 0_Nonblock");
		close(fd);
		return -1;
	}

	sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(port);
	// при addr.sin_port = htons(0); ядро само выбирает порт
	// getsockname(fd, (sockaddr*)&address, &len) == 0)
	// std::cout << "bound to port " << ntohs(got.sin_port) << "\n";


	if (bind(fd, (sockaddr*)&address, sizeof(address)) < 0) {
		std::perror("bind");
		close(fd);
		return -1;
	}

	int backlog = 128;

	if(listen(fd, backlog) < 0) {
		std::perror("listen");
		close(fd);
		return -1;
	}
	std::cout << "Listening...\n";
	return fd;
}

static void close_and_remove(int fd, std::vector<int>& clients,
							std::map<int,std::string>& inbuf,
							std::map<int,std::string>& outbuf) {
    close(fd);
    clients.erase(std::remove(clients.begin(), clients.end(), fd), clients.end());
	inbuf.erase(fd);
	outbuf.erase(fd);
	std::cout << "Close fd=" << fd << "\n";
}

int main(void) {

	signal(SIGPIPE, SIG_IGN);
	std::vector<int> clients;
	std::vector<pollfd> pfds;
	std::map<int, std::string> inbuf;
	std::map<int, std::string> outbuf;


	int port = 6667;
	int server_fd = create_listen_socket(port);
	if (server_fd < 0)
		return 1;

	for(;;) {
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
				set_nonblocking(cfd);
				std::cout << "New client fd=" << cfd << "\n";
				clients.push_back(cfd);
				inbuf[cfd] = "";
				outbuf[cfd] = "";
				continue;
			}
			if (errno == EAGAIN || errno == EWOULDBLOCK)
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

					for(;;) {
							std::string line;
							if (!cut_line(inbuf[fd], line, debug_lf_mode())) break;
							std::cout << "LINE fd=" << fd << " : \"" << line << "\"\n";
							outbuf[fd] += "You said: " + line + "\r\n";
					}
					continue;
				}
				if (n == 0) {
					std::cout << "EOF fd=" << fd << "\n";
					close_and_remove(fd, clients, inbuf, outbuf);
					break;
				}

				if(errno == EAGAIN || errno == EWOULDBLOCK) {
					break;
				}
				std::perror("recv");
				close_and_remove(fd, clients, inbuf, outbuf);
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
						//later
					} else {
						std::perror("send");
						close_and_remove(fd, clients, inbuf, outbuf);
					}
				}
			}
		}

		if (re & (POLLERR | POLLHUP | POLLNVAL)) {
			close_and_remove(fd, clients, inbuf, outbuf);
			continue;
		}
	}

}
	close(server_fd);
	return 0;
}