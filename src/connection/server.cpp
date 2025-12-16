#include "connection/server.hpp"

#include "utils/exception.hpp"
#include "utils/logger.hpp"
#include "core/network.hpp"
#include "connection/client.hpp"
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

namespace connection
{
	ServerSocket::ServerSocket(const int port, const std::string &host, const config::ServerConfig &serverConfig) : Connection(-1, Connection::SERVER_SOCKET), _port(port), _host(host), _serverConfig(serverConfig)
	{
		if (_port <= 0 || _port >= 65535)
			EXCEPTION("Invalid port number: %d", _port);

		std::memset(&_address, 0, sizeof(_address));
		_address.sin_family = AF_INET;
		_address.sin_port = htons(static_cast<in_port_t>(_port));
		if (_host.empty() || _host == "localhost")
			_address.sin_addr.s_addr = htonl(INADDR_ANY);
		else
		{
			if (inet_pton(AF_INET, _host.c_str(), &_address.sin_addr) <= 0)
				EXCEPTION("Invalid host address: %s", _host.c_str());
		}

		const int socketFd = socket(AF_INET, SOCK_STREAM, 0);
		if (socketFd < 0)
			EXCEPTION("Failed to create socket: %s", std::strerror(errno));
		setFd(socketFd);

		int opt = 1;
		if (setsockopt(getFd(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
			EXCEPTION("Failed to set socket options: %s", std::strerror(errno));

		if (bind(getFd(), reinterpret_cast<sockaddr *>(&_address), sizeof(_address)) < 0)
			EXCEPTION("Failed to bind socket to %s:%d: %s", _host.c_str(), _port, std::strerror(errno));

		if (listen(getFd(), SOMAXCONN) < 0)
			EXCEPTION("Failed to listen on socket: %s", std::strerror(errno));

		LOG_INFO("Server socket bound to http://%s:%d", _host.c_str(), _port);
	}

	ServerSocket::~ServerSocket()
	{
	}

	void ServerSocket::handleEvents(short events)
	{
		if (events & (POLLHUP | POLLERR | POLLNVAL))
			LOG_ERROR("Error on events & (POLLHUP | POLLERR | POLLNVAL) on Server fd=%d", getFd());
		if (events & POLLIN)
			handlePollIn();
		if (events & POLLOUT)
			LOG_INFO("event POLLOUT on Server fd=%d", getFd());
	}

	int ServerSocket::acceptClient()
	{
		struct sockaddr_in addr;
		socklen_t addrLen = sizeof(addr);

		int clientFd = accept(getFd(), reinterpret_cast<struct sockaddr *>(&addr), &addrLen);
		if (clientFd < 0)
		{
			if (errno != EWOULDBLOCK && errno != EAGAIN)
				LOG_ERROR("Server acceptation failed: %s", std::strerror(errno));
			return -1;
		}

		char clientIp[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr.sin_addr, clientIp, INET_ADDRSTRLEN);
		LOG_INFO("New client connected: %s:%d (fd=%d)", clientIp, ntohs(addr.sin_port), clientFd);

		return clientFd;
	}

	void ServerSocket::handlePollIn()
	{
		const int clientFd = acceptClient();

		if (clientFd >= 0)
		{
			core::Network *network = core::Network::getInstance();
			ClientSocket *connection = new ClientSocket(clientFd, _serverConfig);
			network->registerConnection(connection, POLLIN);
		}
	}
} // namespace connection
