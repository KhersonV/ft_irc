#include "Cmd_core.hpp"
#include "Net.hpp"
#include "utils.hpp"
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

// outgoing packets from server (to a client)
void	handle_write_ready(int fd, std::map<int, Client> &clients,
		std::vector<int> &fds)
{
	ssize_t	n;

	std::map<int, Client>::iterator it = clients.find(fd);
	if (it == clients.end())
		return ;
	Client &c = it->second;
	if (c.out.empty())
		return ;
	std::cout << "c.out.size() = " << c.out.size() << std::endl; 
	n = send(fd, c.out.c_str(), c.out.size(), 0);
	std::cout << "bytes sent = " << n << std::endl;
	if (n > 0)
	{
		c.out.erase(0, n);
		return ;
	}
	if (n < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return ;
		std::perror("send");
		ftirc::close_and_remove(fd, fds, clients);
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
				if (process_line(fd, line, clients, fds))
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

void add_client(int cfd,
	std::vector<int>& fds, std::map<int, Client>& clients)
{
	ftirc::set_nonblocking(cfd);
	std::cout << "New client fd=" << cfd << "\n";
	fds.push_back(cfd);
	clients.insert(std::make_pair(cfd, Client(cfd)));
}
