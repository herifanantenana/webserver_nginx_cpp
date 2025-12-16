#pragma once

#include <ctime>

namespace connection
{
	class Connection
	{
	public:
		enum ConnectionType
		{
			SERVER_SOCKET,
			CLIENT_SOCKET
		};

	private:
		int _fd;
		const ConnectionType _type;
		time_t _lastActivity;

	public:
		Connection(const int fd, const ConnectionType type);
		virtual ~Connection();

		inline void setFd(const int fd) { _fd = fd; }
		inline void updateActivity() { _lastActivity = std::time(NULL); }

		inline const int &getFd() const { return _fd; }
		inline const ConnectionType &getType() const { return _type; }

		bool isTimedOut(const int sec = 10) const;

		virtual bool shouldClose() const = 0;
		virtual void handleEvents(short events) = 0;
	};
} // namespace connection
