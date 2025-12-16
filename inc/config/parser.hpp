#pragma once

#include <string>
#include "config/server.hpp"
#include "config/location.hpp"
#include "config/constant.hpp"

namespace config
{
	class ParserConfig
	{
	private:
		std::string _configFilePath;
		std::vector<ServerConfig> _servers;

		bool cleanUpAndContinue(std::string &line);
		void parseLocationBlock(std::ifstream &file, LocationConfig &locationConfig);
		void parseServerBlock(std::ifstream &file, ServerConfig &serverConfig);

	public:
		ParserConfig(const std::string &configFilePath);
		~ParserConfig();

		inline std::vector<ServerConfig> getServer() const { return _servers; }

		void parseConfigFile();
		void setup();
		void printConfig(const std::string &outputFilePath) const;
	};
} // namespace config
