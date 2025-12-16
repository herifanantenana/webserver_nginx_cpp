#pragma once

#include <netinet/in.h>
#include "connection/connection.hpp"
#include "config/server.hpp"

namespace connection
{
	class ServerSocket : public Connection
	{
	private:
		const int _port;
		const std::string _host;
		const config::ServerConfig &_serverConfig;
		sockaddr_in _address;

	public:
		ServerSocket(const int port, const std::string &host, const config::ServerConfig &serverConfig);
		virtual ~ServerSocket();

		virtual void handleEvents(short events);

		inline virtual bool shouldClose() const { return false; }
	};
} // namespace connection
