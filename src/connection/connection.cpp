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
} // namespace connection
