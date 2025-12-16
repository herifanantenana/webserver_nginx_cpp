#pragma once

#include <string>
#include <vector>

namespace utils
{
	bool pathExists(const std::string &path);
	bool isDirectory(const std::string &path);
	bool isRegularFile(const std::string &path);
	bool isReadable(const std::string &path);
	bool isWritable(const std::string &path);
	bool isExecutable(const std::string &path);

	void trimChars(std::string &str, const std::string &chars = " \t\n\r\f\v");
	bool startsWith(const std::string &str, const std::string &prefix, const std::string &after);
	void removeExtraChar(std::string &str, const char ch);
	std::vector<std::string> splitString(const std::string &str, const char &del);
	std::string buildPath(const std::string &path1, const std::string &path2);

	size_t hexToSizeT(const std::string &hexStr);
} // namespace utils
