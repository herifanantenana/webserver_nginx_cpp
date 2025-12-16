#include "connection/client.hpp"

#include "utils/logger.hpp"
#include "utils/utils.hpp"
#include "http/request.hpp"
#include "config/location.hpp"
#include <poll.h>
#include <cstring>
#include <sys/socket.h>
#include <cerrno>

namespace connection
{
	ClientSocket::ClientSocket(const int fd, const config::ServerConfig &serverConfig)
			: Connection(fd, Connection::CLIENT_SOCKET),
				_serverConfig(serverConfig),
				_state(READING_REQUEST),
				_request(),
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
			handlePollIn();
		if (events & POLLOUT)
			LOG_INFO("event POLLOUT on Client fd=%d", getFd());
	}

	void ClientSocket::identifyRequestType()
	{
		LOG_DEBUG("Identifying request type for Client fd=%d", getFd());

		const config::LocationConfig *location = _serverConfig.getLocationForRequest(_request.getUri());

		if (location)
		{
			if (!location->getRedirect().second.empty())
			{
				_request.setReqType(http::HttpRequest::REQ_REDIRECT);
				LOG_CONSOLE("ClientSocket fd: %d request identified as REDIRECT", getFd());
				return;
			}
			if (_request.isCgiRequest(location->getCgiMappings()))
			{
				_request.setReqType(http::HttpRequest::REQ_CGI);
				LOG_CONSOLE("ClientSocket fd: %d request identified as CGI", getFd());
				return;
			}

			// todo: check upload paths
		}

		_request.setReqType(http::HttpRequest::REQ_STATIC);
		LOG_CONSOLE("ClientSocket fd: %d request identified as STATIC", getFd());
	}

	void ClientSocket::handlePollIn()
	{
		updateActivity();

		char buffer[4096];
		ssize_t bytesRead = recv(getFd(), buffer, sizeof(buffer), 0);
		if (bytesRead < 0)
		{
			LOG_ERROR("ClientSocket fd: %d recv error : %s", getFd(), std::strerror(errno));
			_state = CLIENT_ERROR;
			return;
		}
		if (bytesRead == 0)
		{
			LOG_WARNING("Client fd=%d closed the connection", getFd());
			_state = CLOSING;
			return;
		}
		buffer[bytesRead] = '\0';

		http::HttpRequest::ParseState parseState = _request.parse(buffer, bytesRead);
		// todo: append to read buffer and process request
		// todo: some stuff

		if (parseState == http::HttpRequest::PARSE_BAD_REQUEST)
		{
			LOG_ERROR("Bad request from client fd=%d", getFd());
			_state = CLIENT_ERROR;
			return;
		}
		if (parseState == http::HttpRequest::PARSE_BODY)
		{
			identifyRequestType();
		}
	}

} // namespace connection
