#include "config/server.hpp"

#include "utils/exception.hpp"
#include "utils/utils.hpp"
#include <iostream>

namespace config
{
	ServerConfig::ServerConfig() : _hostPorts(), _rootPath(""), _errorPages(), _clientMaxBodySize(0), _locations()
	{
	}

	ServerConfig::~ServerConfig()
	{
	}

	const LocationConfig *ServerConfig::getLocationForRequest(const std::string &requestUri) const
	{
		const LocationConfig *bestMatch = NULL;
		size_t bestMatchLength = 0;

		for (std::vector<LocationConfig>::const_iterator it = _locations.begin(); it != _locations.end(); ++it)
		{
			if (utils::startsWith(requestUri, it->getAliasPath(), "/"))
			{
				if (it->getAliasPath().length() > bestMatchLength)
				{
					bestMatch = &(*it);
					bestMatchLength = it->getAliasPath().length();
				}
			}
		}

		return bestMatch;
	}

	void ServerConfig::setup()
	{
		if (_hostPorts.empty())
			EXCEPTION("Server must have at least one listen info");
		for (std::vector<HostPort>::const_iterator it = _hostPorts.begin(); it != _hostPorts.end(); ++it)
		{
			if (it->second <= 0 || it->second > 65535)
				EXCEPTION("Invalid port number in listen info: %d", it->second);
		}

		if (_rootPath.empty())
			EXCEPTION("Server root path cannot be empty");

		if (_errorPages.empty())
			_errorPages[500] = "500.html";
		for (std::map<int, std::string>::iterator it = _errorPages.begin(); it != _errorPages.end(); ++it)
		{
			if (it->first < 400 || it->first > 599)
				EXCEPTION("Invalid HTTP status code for error page: %d", it->first);
			it->second = utils::buildPath(_rootPath, it->second);
		}

		if (_locations.empty())
			EXCEPTION("Server must have at least one location");
		for (std::vector<LocationConfig>::iterator it = _locations.begin(); it != _locations.end(); ++it)
			it->setup(_rootPath);
	}

	void config::ServerConfig::printConfig(std::ofstream &outFile) const
	{
		outFile << "Server Config:" << std::endl;

		outFile << "\tHost and Ports:" << std::endl;
		for (std::vector<HostPort>::const_iterator it = _hostPorts.begin(); it != _hostPorts.end(); ++it)
			outFile << "\t\t- " << it->first << ":" << it->second << std::endl;

		outFile << "\tRoot Path: " << _rootPath << std::endl;
		outFile << "\tError Pages:" << std::endl;
		for (std::map<int, std::string>::const_iterator it = _errorPages.begin(); it != _errorPages.end(); ++it)
			outFile << "\t\t- " << it->first << ": " << it->second << std::endl;

		outFile << "\tClient Max Body Size: " << _clientMaxBodySize << std::endl;

		outFile << "\tLocations:" << std::endl;
		for (std::vector<LocationConfig>::const_iterator it = _locations.begin(); it != _locations.end(); ++it)
			it->printConfig(outFile);
	}
} // namespace config
