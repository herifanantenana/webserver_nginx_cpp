#include "core/network.hpp"

#include <csignal>
#include <cstring>
#include <cerrno>
#include "utils/logger.hpp"
#include "utils/exception.hpp"
#include "connection/server.hpp"
#include "connection/client.hpp"

namespace core
{
	Network *Network::_instance = NULL;
	bool Network::_isRunning = false;

	void Network::handleSignal(int signal)
	{
		if (signal == SIGINT || signal == SIGTERM)
		{
			LOG_INFO("Signal %d received, shutting down network...", signal);
			Network::_isRunning = false;
		}
	}

	Network::Network() : _serverConfigs(), _pollFds(), _connections()
	{
	}

	Network::~Network()
	{
		for (std::map<const int, connection::Connection *>::iterator it = _connections.begin(); it != _connections.end(); ++it)
			delete it->second;

		_serverConfigs.clear();
		_pollFds.clear();
		_connections.clear();
	}

	Network *Network::getInstance()
	{
		if (_instance == NULL)
			_instance = new Network();

		return _instance;
	}

	void Network::destroyInstance()
	{
		if (_instance != NULL)
		{
			delete _instance;
			_instance = NULL;
			_isRunning = false;
		}
	}

	void Network::registerConnection(connection::Connection *connection, short events)
	{
		const int fd = connection->getFd();
		if (fd < 0)
			EXCEPTION("Cannot register connection with invalid file descriptor %d.", fd);

		_connections.insert(std::make_pair(fd, connection));

		pollfd pfd;
		pfd.fd = fd;
		pfd.events = events;
		pfd.revents = 0;
		_pollFds.push_back(pfd);
	}

	void Network::unregisterConnection(connection::Connection *connection)
	{
		const int fd = connection->getFd();
		connection::Connection *conn = getConnectionFd(fd);
		if (conn)
		{
			_connections.erase(conn->getFd());
			delete conn;

			for (std::vector<pollfd>::iterator pfdIt = _pollFds.begin(); pfdIt != _pollFds.end(); ++pfdIt)
			{
				if (pfdIt->fd == fd)
				{
					_pollFds.erase(pfdIt);
					break;
				}
			}

			LOG_FATAL("Closing Connection fd=%d", fd);
		}
		else
			LOG_WARNING("Attempted to unregister non-existent connection with fd %d.", fd);
	}

	connection::Connection *core::Network::getConnectionFd(const int fd)
	{
		std::map<const int, connection::Connection *>::iterator connIt = _connections.find(fd);
		if (connIt != _connections.end())
			return connIt->second;
		return NULL;
	}

	void Network::setupServer()
	{
		for (std::vector<config::ServerConfig>::const_iterator it = _serverConfigs.begin(); it != _serverConfigs.end(); ++it)
		{
			std::vector<config::ServerConfig::HostPort> hostPorts = it->getHostPort();
			if (hostPorts.empty())
				EXCEPTION("Server configuration has no ports defined.");

			for (std::vector<config::ServerConfig::HostPort>::const_iterator hpIt = hostPorts.begin(); hpIt != hostPorts.end(); ++hpIt)
			{
				connection::ServerSocket *serverSocket = NULL;
				try
				{
					serverSocket = new connection::ServerSocket(hpIt->second, hpIt->first, *it);
					registerConnection(serverSocket, POLLIN);
				}
				catch (const std::exception &e)
				{
					if (serverSocket)
						delete serverSocket;
					HANDLE_EXCEPTION(e);
				}
			}
		}

		if (_connections.empty())
			EXCEPTION("No server sockets were created. Check server configurations.");
	}

	void Network::synchronizePollFds()
	{
		for (std::vector<pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it)
		{
			const int fd = it->fd;
			connection::Connection *conn = getConnectionFd(fd);
			if (!conn)
			{
				LOG_WARNING("Pollfd with fd %d has no associated connection. Skipping.", fd);
				_pollFds.erase(it);
				continue;
			}

			short events = POLLIN | POLLHUP | POLLERR;

			if (conn->getType() == connection::Connection::CLIENT_SOCKET)
			{
				connection::ClientSocket *clientSocket = dynamic_cast<connection::ClientSocket *>(conn);
				if (clientSocket)
				{
					connection::ClientSocket::ClientState clientState = clientSocket->getState();
					if (clientState == connection::ClientSocket::PROCESSING_REQUEST || clientState == connection::ClientSocket::EXECUTING_CGI || clientState == connection::ClientSocket::WRITING_RESPONSE)
						events |= POLLOUT;
				}
				else
					LOG_WARNING("Pollfd with fd %d is not associated with a ClientSocket. Skipping.", fd);
			}

			it->events = events;
		}
	}

	void Network::handlePollEvents(int eventCount)
	{
		std::vector<connection::Connection *> toRemove;
		for (size_t i = 0; i < _pollFds.size() && eventCount > 0; ++i)
		{
			if (_pollFds[i].revents == 0)
				continue;

			--eventCount;
			int fd = _pollFds[i].fd;
			short events = _pollFds[i].revents;

			connection::Connection *connection = getConnectionFd(fd);
			if (!connection)
				continue;

			try
			{
				connection->handleEvents(events);
				if (connection->shouldClose())
					toRemove.push_back(connection);
			}
			catch (const std::exception &e)
			{
				HANDLE_EXCEPTION(e);
				toRemove.push_back(connection);
			}

			for (std::vector<connection::Connection *>::iterator it = toRemove.begin(); it != toRemove.end(); ++it)
				unregisterConnection(*it);
			toRemove.clear();
		}
	}

	void Network::cleanUpTimeOutConnection()
	{
		std::vector<connection::Connection *> toRemove;

		for (std::map<const int, connection::Connection *>::iterator connIt = _connections.begin(); connIt != _connections.end(); ++connIt)
		{
			connection::Connection *conn = connIt->second;

			if (conn->getType() == connection::Connection::SERVER_SOCKET)
				continue;

			if (conn->isTimedOut(10))
			{
				// todo: handle timeout response
				LOG_WARNING("Connection timeOut fd=%d", connIt->first);
				toRemove.push_back(conn);
			}
		}

		for (std::vector<connection::Connection *>::iterator it = toRemove.begin(); it != toRemove.end(); ++it)
			unregisterConnection(*it);
		toRemove.clear();
	}

	void Network::init(std::vector<config::ServerConfig> serverConfigs)
	{
		if (serverConfigs.empty())
			EXCEPTION("No server configurations provided to initialize the network.");

		signal(SIGINT, handleSignal);
		signal(SIGTERM, handleSignal);

		_serverConfigs = serverConfigs;
	}

	void Network::run()
	{
		LOG_INFO("Starting network...");
		setupServer();
		_isRunning = true;

		while (_isRunning)
		{
			synchronizePollFds();

			int eventCount = poll(&_pollFds[0], _pollFds.size(), 0);

			if (eventCount < 0)
			{
				if (errno == EINTR)
					continue;
				EXCEPTION("Poll error: %s", std::strerror(errno));
			}

			if (eventCount > 0)
				handlePollEvents(eventCount);

			cleanUpTimeOutConnection();
		}
	}
} // namespace core
