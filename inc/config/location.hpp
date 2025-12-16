#pragma once

#include <string>
#include <vector>

namespace config
{
	class LocationConfig
	{
	private:
		typedef std::pair<std::string, std::string> CgiMapping;

		std::string _aliasPath;
		std::string _rootPath;
		bool _autoIndex;
		std::vector<std::string> _indexFiles;
		std::vector<std::string> _allowedMethods;
		std::vector<CgiMapping> _cgiMappings;
		std::vector<std::string> _uploadPaths;
		std::pair<int, std::string> _redirect;

	public:
		LocationConfig();
		~LocationConfig();

		inline void setAliasPath(const std::string &path) { _aliasPath = path; }
		inline void setRootPath(const std::string &path) { _rootPath = path; }
		inline void setAutoIndex(bool autoIndex) { _autoIndex = autoIndex; }
		inline void addIndexFile(const std::string &file) { _indexFiles.push_back(file); }
		inline void addAllowedMethod(const std::string &method) { _allowedMethods.push_back(method); }
		inline void addCgiMapping(const std::string &extension, const std::string &script) { _cgiMappings.push_back(std::make_pair(extension, script)); }
		inline void addUploadPath(const std::string &path) { _uploadPaths.push_back(path); }
		inline void setRedirect(int code, const std::string &url) { _redirect = std::make_pair(code, url); }

		void setup(const std::string &serverRootPath);
		void printConfig(std::ofstream &outFile) const;
	};
} // namespace config
