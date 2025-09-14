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


namespace {

int parse_args(int argc, char** argv) {
	int port = 6667;
	if (argc >= 2) port = std::atoi(argv[1]);
	if (argc >= 3) g_state.server_password = argv[2];
	else           g_state.server_password = "secret";
	return port;
}


// rebuilds the list of fds that poll() should watch (listener + clients)
// poll() needs an up-to-date array each loop to know what to wait for
void build_pollfds(int server_fd, const std::vector<int>& fds, const std::map<int, Client>& clients, std::vector<pollfd>& pfds)
{
	pfds.clear();
	pfds.reserve(1 + fds.size());
	pollfd p;
	p.fd = server_fd;
	p.events = POLLIN;
	p.revents = 0;
	pfds.push_back(p);

	for (size_t i = 0; i < fds.size(); ++i) {
		int cfd = fds[i];
		short event = POLLIN; // default to always read (listen) incoming data
		std::map<int, Client>::const_iterator it = clients.find(cfd);
		if (it != clients.end() && !it->second.out.empty()) // if there's data to send, also watch for write readiness
			event |= POLLOUT; // |= keeps the POLLIN flag as well, kinda like +=
		p.fd = cfd;
		p.events = event;
		p.revents = 0;
		pfds.push_back(p);
	}
}

// accepts all queued incoming connections (non-blocking)
// to turn readiness on the listen socket into new client fds
void register_new_clients(int server_fd, std::vector<int>& fds, std::map<int, Client>& clients)
{
	// todo will this always print to stderror no matter what??
	for (;;) {
		int cfd = accept(server_fd, 0, 0);
		if (cfd >= 0) {
			add_client(cfd, fds, clients);
			continue;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK) break;
		std::perror("accept");
		break;
	}
}
// handles single clients interactions
void handle_one_fd(int fd, short revents, std::vector<int>& fds, std::map<int, Client>& clients)
{
	bool closed = false;

	if (revents & POLLIN) { // if revents has POLLIN flag set
		closed = handle_read_ready(fd, clients, fds);
	}
	if (closed) return;
	if (revents & POLLOUT) { // if revents has POLLOUT flag set
		handle_write_ready(fd, clients, fds);
	}
	if (revents & (POLLERR | POLLHUP | POLLNVAL)) { // if revents has any of these flags set
		ftirc::close_and_remove(fd, fds, clients);
	}
}

void handle_events(const std::vector<pollfd>& pfds, std::vector<int>& fds, std::map<int, Client>& clients)
{
	for (size_t i = 1; i < pfds.size(); ++i)
		handle_one_fd(pfds[i].fd, pfds[i].revents, fds, clients);
}

} // anonymous namespace

int main(int argc, char** argv)
{
	signal(SIGPIPE, SIG_IGN);

	std::vector<pollfd> pfds;
	std::vector<int>    fds;
	std::map<int, Client> clients;

	int port = parse_args(argc, argv);

	int server_fd = ftirc::create_listen_socket(port);
	if (server_fd < 0) {
		std::perror("create_listen_socket");
		return 1;
	}

	for (;;) { // eternal server loop
		build_pollfds(server_fd, fds, clients, pfds);

		int ret = poll(&pfds[0], pfds.size(), -1); // sets revents in pfds
		if (ret < 0) {
			if (errno == EINTR) continue;
			std::perror("poll");
			break;
		}

		if (pfds[0].revents & POLLIN) // if the server is ready to accept new connections
			register_new_clients(server_fd, fds, clients);

		handle_events(pfds, fds, clients);
	}

	close(server_fd);
	return 0;
}
