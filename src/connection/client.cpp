#include "connection/client.hpp"

#include "utils/logger.hpp"
#include <poll.h>

namespace connection
{
	ClientSocket::ClientSocket(const int fd, const config::ServerConfig &serverConfig)
			: Connection(fd, Connection::CLIENT_SOCKET),
				_serverConfig(serverConfig),
				_state(READING_REQUEST),
				// ? request
				// ? response
				_readBuffer(),
				_writeBuffer(),
				_writeOffset(),
				_isKeepAlive(false),
				_requestCount(0)
	// ? cgi handler
	{
	}

	ClientSocket::~ClientSocket()
	{
	}

	bool ClientSocket::shouldClose() const
	{
		return _state == ClientSocket::CLOSING || _state == ClientSocket::CLIENT_ERROR;
	}

	void ClientSocket::handleEvents(short events)
	{
		if (events & (POLLHUP | POLLERR | POLLNVAL))
			LOG_ERROR("Error on events & (POLLHUP | POLLERR | POLLNVAL) on Client fd=%d", getFd());
		if (events & POLLIN)
			LOG_INFO("event POLLIN on Client fd=%d", getFd());
		if (events & POLLOUT)
			LOG_INFO("event POLLOUT on Client fd=%d", getFd());
	}

	void ClientSocket::handlePollIn()
	{
	}

} // namespace connection
