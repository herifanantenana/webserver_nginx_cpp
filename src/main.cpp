#include "main.hpp"

int main(int argc, char const *argv[])
{
	if (argc != 2)
	{
		LOG_ERROR("Usage: %s <config_file>", argv[0]);
		return EXIT_FAILURE;
	}
	try
	{
		config::ParserConfig parserConfig(argv[1]);
		parserConfig.parseConfigFile();
		parserConfig.setup();
		parserConfig.printConfig(std::string(argv[1]) + ".out");
		std::vector<config::ServerConfig> serverConfigs = parserConfig.getServer();

		core::Network *network = core::Network::getInstance();
		network->init(serverConfigs);
		network->run();
		core::Network::destroyInstance();
	}
	catch (const std::exception &e)
	{
		HANDLE_EXCEPTION(e);
	}

	return EXIT_SUCCESS;
}
