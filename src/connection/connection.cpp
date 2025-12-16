#include "connection/connection.hpp"

#include <unistd.h>
#include "utils/logger.hpp"

namespace connection
{
	Connection::Connection(const int fd, const ConnectionType type) : _fd(fd), _type(type), _lastActivity(std::time(NULL))
	{
	}

	Connection::~Connection()
	{
		if (_fd >= 0)
		{
			if (close(_fd) < 0)
				LOG_WARNING("Setting an invalid fd: %d", _fd);
		}
	}

	bool connection::Connection::isTimedOut(const int sec) const
	{
		if (_lastActivity == 0)
			return false;

		time_t now = std::time(NULL);
		return (now - _lastActivity) > sec;
	}
} // namespace connection
