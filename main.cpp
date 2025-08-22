#include <sys/socket.h>
#include <netinet/in.h>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

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

int main(void) {

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

	// SO_REUSEADDR тест, удалить
	int val = 0;
	socklen_t len = sizeof(val);
	if (getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, &len) == -1) {
		std::perror("gertsockopt SO_REUSEADDR");
	} else {
		std::cout << "SO_REUSEADDR: " << val << "\n";
	}

	if (set_nonblocking(fd) < 0) {
		std::perror("fcntl 0_Nonblock");
		close(fd);
		return -1;
	}

	// Non-Block тест, удалить
	int f2 = fcntl(fd, F_GETFL, 0);
	if (f2 == -1) {
		std::perror("fcntl F_GETFL (after)");
	} else {
	 std::cout << "nonblock: " << ((f2 & O_NONBLOCK) ? "ON" : "OFF") << "\n";
	}



	sockaddr_in address;
	int port = 6667;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET; //ipv4
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(port);

	// при addr.sin_port = htons(0); ядро само выбирает порт
	// getdockname(fd, (sockaddr*)&address, &len) == 0)
	// std::cout << "bound to port " << ntohs(got.sin_port) << "\n";

	if (bind(fd, (sockaddr*)&address, sizeof(address)) < 0) {
		std::perror("bind");
		close(fd);
		return -1;
	}


	close(fd);

	return 0;
}