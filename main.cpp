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

static void close_and_remove(int fd, std::vector<int>& clients) {
    ::close(fd);
    clients.erase(std::remove(clients.begin(), clients.end(), fd), clients.end());
    std::cout << "Close fd=" << fd << "\n";
}

int main(void) {
	std::vector<int> clients;
	std::vector<pollfd> pfds;


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
				continue;
			}
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
		}
	}

	for (size_t i = 1; i < pfds.size(); ++i) {
		int fd = pfds[i].fd;
		short re = pfds[i].revents;

		if (re & (POLLERR | POLLHUP | POLLNVAL)) {
			close_and_remove(fd, clients);
			continue;
		}

		if (re & POLLIN) {
			char buf[4096];
			for (;;) {
				ssize_t n = recv(fd, buf, sizeof(buf), 0);
				if (n > 0) {
					std::cout << "Read fd=" << fd << " bytes=" << n
							<< " data=\"" << std::string(buf, n) << "\"\n";
					continue;
				}
				if (n == 0) {
					std::cout << "EOF fd=" << fd << "\n";
					close_and_remove(fd, clients);
					break;
				}

				if(errno == EAGAIN || errno == EWOULDBLOCK) {
					break;
				}
				std::perror("recv");
				close_and_remove(fd, clients);
				break;
			}
		}
	}

}
	close(server_fd);
	return 0;
}