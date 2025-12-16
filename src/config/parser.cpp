#include "config/parser.hpp"

#include <fstream>
#include <cstdlib>
#include "utils/exception.hpp"
#include "utils/utils.hpp"
#include "utils/logger.hpp"

namespace config
{
	ParserConfig::ParserConfig(const std::string &configFilePath) : _configFilePath(configFilePath), _servers()
	{
	}

	ParserConfig::~ParserConfig()
	{
	}

	bool ParserConfig::cleanUpAndContinue(std::string &line)
	{
		utils::trimChars(line);
		if (line.empty() || line[0] == '#')
			return true;
		utils::removeExtraChar(line, ';');
		return false;
	}

	void ParserConfig::parseLocationBlock(std::ifstream &file, LocationConfig &locationConfig)
	{
		std::string line;

		while (std::getline(file, line))
		{
			if (cleanUpAndContinue(line))
				continue;

			if (line == "}")
				break;

			std::vector<std::string> tokens = utils::splitString(line, ' ');
			if (tokens.empty())
				continue;
			if (tokens.size() < 2)
				EXCEPTION("Expected arguments for directive: %s", tokens[0].c_str());

			const std::string directive = tokens.front();
			if (directive == ROOT_DIRECTIVE)
			{
				locationConfig.setRootPath(tokens[1]);
			}
			else if (directive == AUTOINDEX_DIRECTIVE)
			{
				const std::string autoIndexArg = tokens[1];
				if (autoIndexArg != "on" && autoIndexArg != "off")
					EXCEPTION("Invalid argument for autoindex directive: %s", autoIndexArg.c_str());

				locationConfig.setAutoIndex(autoIndexArg == "on");
			}
			else if (directive == INDEX_DIRECTIVE)
			{
				for (size_t i = 1; i < tokens.size(); i++)
					locationConfig.addIndexFile(tokens[i]);
			}
			else if (directive == ALLOW_METHODS_DIRECTIVE)
			{
				for (size_t i = 1; i < tokens.size(); i++)
					locationConfig.addAllowedMethod(tokens[i]);
			}
			else if (directive == CGI_DIRECTIVE)
			{
				if (tokens.size() != 3)
					EXCEPTION("Expected exactly two arguments for cgi directive: %s", line.c_str());
				const std::string extension = tokens[1];
				const std::string scriptPath = tokens[2];

				locationConfig.addCgiMapping(extension, scriptPath);
			}
			else if (directive == UPLOAD_DIRECTIVE)
			{
				for (size_t i = 1; i < tokens.size(); i++)
					locationConfig.addUploadPath(tokens[i]);
			}
			else if (directive == REDIRECT_DIRECTIVE)
			{
				const int code = (tokens.size() > 2) ? std::atoi(tokens[1].c_str()) : 302;
				const std::string url = (tokens.size() > 2) ? tokens[2] : tokens[1];

				locationConfig.setRedirect(code, url);
			}
			else
			{
				EXCEPTION("Unknown directive in location block: %s", directive.c_str());
			}
		}
	}

	void ParserConfig::parseServerBlock(std::ifstream &file, ServerConfig &serverConfig)
	{
		std::string line;

		while (std::getline(file, line))
		{
			if (cleanUpAndContinue(line))
				continue;

			if (line == "}")
				break;

			std::vector<std::string> tokens = utils::splitString(line, ' ');
			if (tokens.empty())
				continue;
			if (tokens.size() < 2)
				EXCEPTION("Expected arguments for directive: %s", tokens[0].c_str());

			const std::string directive = tokens.front();
			if (directive == LISTEN_DIRECTIVE)
			{
				const std::string listenArg = tokens[1];
				size_t colonPos = listenArg.find(':');
				const std::string host = (colonPos != std::string::npos) ? listenArg.substr(0, colonPos) : "127.0.0.1";
				const int port = (colonPos != std::string::npos) ? std::atoi(listenArg.substr(colonPos + 1).c_str()) : std::atoi(listenArg.c_str());

				serverConfig.addHostPort(host, port);
			}
			else if (directive == ROOT_DIRECTIVE)
			{
				serverConfig.setRootPath(tokens[1]);
			}
			else if (directive == ERROR_PAGE_DIRECTIVE)
			{
				if (tokens.size() < 3)
					EXCEPTION("Expected error code and path for error_page directive: %s", line.c_str());
				const int errorCode = std::atoi(tokens[1].c_str());
				const std::string errorPagePath = tokens[2];

				serverConfig.addErrorPage(errorCode, errorPagePath);
			}
			else if (directive == CLIENT_MAX_BODY_SIZE_DIRECTIVE)
			{
				size_t multiplier = 1;
				std::string sizeStr = tokens[1];
				char lastChar = sizeStr.at(sizeStr.size() - 1);
				if (lastChar == 'K' || lastChar == 'k')
					multiplier = 1024;
				else if (lastChar == 'M' || lastChar == 'm')
					multiplier = 1024 * 1024;
				else if (lastChar == 'G' || lastChar == 'g')
					multiplier = 1024 * 1024 * 1024;
				if (multiplier != 1)
					sizeStr.erase(sizeStr.size() - 1);

				serverConfig.setClientMaxBodySize(static_cast<size_t>(std::atoi(sizeStr.c_str())) * multiplier);
			}
			else if (directive == LOCATION_DIRECTIVE)
			{
				std::string locationArg = tokens[1];
				if (line.find('{') == std::string::npos)
					EXCEPTION("Expected '{' after location directive");
				utils::removeExtraChar(locationArg, '{');

				LocationConfig locationConfig;
				locationConfig.setAliasPath(locationArg);
				parseLocationBlock(file, locationConfig);
				serverConfig.addLocation(locationConfig);
			}
			else
			{
				EXCEPTION("Unknown directive in server block: %s", directive.c_str());
			}
		}
	}

	void ParserConfig::parseConfigFile()
	{
		if (_configFilePath.empty())
			EXCEPTION("Config file path is empty");

		std::ifstream file(_configFilePath.c_str());
		if (!file.is_open())
			EXCEPTION("Failed to open config file: %s", _configFilePath.c_str());

		std::string line;
		while (std::getline(file, line))
		{
			if (cleanUpAndContinue(line))
				continue;

			if (utils::startsWith(line, SERVER_DIRECTIVE, " \t{"))
			{
				if (line.find('{') == std::string::npos)
					EXCEPTION("Expected '{' after server directive");

				ServerConfig serverConfig;
				parseServerBlock(file, serverConfig);
				_servers.push_back(serverConfig);
			}
		}
	}

	void ParserConfig::setup()
	{
		if (_servers.empty())
			EXCEPTION("No server blocks found in configuration");
		for (std::vector<ServerConfig>::iterator it = _servers.begin(); it != _servers.end(); ++it)
			it->setup();
	}

	void ParserConfig::printConfig(const std::string &outputFilePath) const
	{
		std::ofstream outFile(outputFilePath.c_str());
		if (!outFile.is_open())
			EXCEPTION("Failed to open output file for writing parsed configuration.");
		outFile << "Parsed Configuration:" << std::endl;
		for (size_t i = 0; i < _servers.size(); ++i)
		{
			outFile << "----------------------------------------" << std::endl;
			outFile << "Server " << i + 1 << ":" << std::endl;
			_servers[i].printConfig(outFile);
		}
		outFile.close();
	}
} // namespace config
