#pragma once

#include <vector>
#include <string>
#include <map>
#include "config/location.hpp"

namespace config
{
	class ServerConfig
	{
	public:
		typedef std::pair<std::string, int> HostPort;

	private:
		std::vector<HostPort> _hostPorts;
		std::string _rootPath;
		std::map<int, std::string> _errorPages;
		size_t _clientMaxBodySize;
		std::vector<LocationConfig> _locations;

	public:
		ServerConfig();
		~ServerConfig();

		inline void addHostPort(const std::string &host, int port) { _hostPorts.push_back(std::make_pair(host, port)); }
		inline void setRootPath(const std::string &path) { _rootPath = path; }
		inline void addErrorPage(int code, const std::string &path) { _errorPages[code] = path; }
		inline void setClientMaxBodySize(size_t size) { _clientMaxBodySize = size; }
		inline void addLocation(const LocationConfig &location) { _locations.push_back(location); }

		inline const std::vector<HostPort> &getHostPorts() const { return _hostPorts; }

		const LocationConfig *getLocationForRequest(const std::string &requestUri) const;
		void setup();
		void printConfig(std::ofstream &outFile) const;
	};
} // namespace config
