#include "connection/client.hpp"

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
} // namespace connection
