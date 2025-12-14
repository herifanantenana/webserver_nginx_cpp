#include "conn/client.hpp"

#include "utils/logger.hpp"
#include "utils/utils.hpp"
#include "config/location.hpp"
#include <poll.h>
#include <sys/socket.h>
#include <cstring>

namespace conn
{
	ClientSocket::ClientSocket(const int fd, const config::ServerConfig &serverConfig) : Connection(fd, Connection::CLIENT_SOCKET), _serverConfig(serverConfig), _state(READING_REQUEST), _request()
	{
	}

	ClientSocket::~ClientSocket()
	{
	}

	void ClientSocket::identifyRequestType()
	{
		const config::LocationConfig *location = _serverConfig.getLocationForRequest(_request.getUri());

		if (location)
		{
			if (!location->getRedirect().second.empty())
			{
				_request.setReqType(http::Request::REQ_REDIRECT);
				LOG_CONSOLE("ClientSocket fd: %d request identified as REDIRECT", getFd());
				return;
			}

			if (_request.isCgiRequest(location->getCgiMappings()))
			{
				_request.setReqType(http::Request::REQ_CGI);
				LOG_CONSOLE("ClientSocket fd: %d request identified as CGI", getFd());
				return;
			}

			if (_request.isUploadRequest(location->getUploadPaths()))
			{
				_request.setReqType(http::Request::REQ_UPLOAD);
				LOG_CONSOLE("ClientSocket fd: %d request identified as UPLOAD", getFd());
				return;
			}

			_request.setReqType(http::Request::REQ_STATIC);
			LOG_CONSOLE("ClientSocket fd: %d request identified as STATIC", getFd());
		}
	}

	void ClientSocket::handlePollIn()
	{
		LOG_DEBUG("ClientSocket fd: %d handlePollIn called", getFd());
		updateLastActivity();
		LOG_FATAL("ClientSocket fd: %d updated last activity time", getFd());

		char buffer[1024];
		ssize_t bytesRead = recv(getFd(), buffer, sizeof(buffer) - 1, 0);
		if (bytesRead < 0)
		{
			LOG_ERROR("ClientSocket fd: %d recv error : %s", getFd(), std::strerror(errno));
			_state = CLIENT_ERROR;
			return;
		}
		if (bytesRead == 0)
		{
			LOG_INFO("ClientSocket fd: %d connection closed by peer", getFd());
			_state = CLOSING;
			return;
		}

		buffer[bytesRead] = '\0';
		http::Request::ParseState parseState = _request.parse(buffer, static_cast<size_t>(bytesRead));

		if (parseState == http::Request::PARSE_ERROR)
		{
			LOG_ERROR("ClientSocket fd: %d request parse error", getFd());
			// !fix: build response parse error
			_state = CLIENT_ERROR;
			// !fix: prepare error response
			return;
		}

		if (parseState == http::Request::PARSE_BODY && !_request.getIsTypeIdentified())
		{
			identifyRequestType();
		}
	}

	void ClientSocket::handlePollOut()
	{
		LOG_DEBUG("ClientSocket fd: %d handlePollOut called", getFd());
	}

	void ClientSocket::handlePollErr()
	{
		LOG_DEBUG("ClientSocket fd: %d handlePollErr called", getFd());
	}

	void ClientSocket::handleEvents(const short events)
	{
		LOG_DEBUG("ClientSocket fd: %d handleEvents called with events: %s", getFd(), utils::getEventNames(events).c_str());
		if (events & POLLIN)
		{
			handlePollIn();
		}
		if (events & POLLOUT)
		{
			handlePollOut();
		}
		if (events & (POLLERR | POLLHUP | POLLNVAL))
		{
			LOG_WARNING("ClientSocket fd: %d received hang up or error event", getFd());
			handlePollErr();
		}
	}
} // namespace conn
