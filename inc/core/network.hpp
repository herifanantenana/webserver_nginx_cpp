#pragma once

#include <vector>
#include <poll.h>
#include "config/server.hpp"
#include "connection/connection.hpp"

namespace core
{
	class Network
	{
	private:
		static Network *_instance;
		static bool _isRunning;
		static void handleSignal(int signal);

		std::vector<config::ServerConfig> _serverConfigs;
		std::vector<pollfd> _pollFds;
		std::map<const int, connection::Connection *> _connections;

		Network();
		~Network();

		void setupServer();
		void synchronizePollFds();
		void handlePollEvents(int eventCount);

	public:
		static Network *getInstance();
		static void destroyInstance();

		void init(std::vector<config::ServerConfig> serverConfigs);
		void run();

		void registerConnection(connection::Connection *connection, short events);
		void unregisterConnection(connection::Connection *connection);
		connection::Connection *getConnectionFd(const int fd);
	};
} // namespace network
