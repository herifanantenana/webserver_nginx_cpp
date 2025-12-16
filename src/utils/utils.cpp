#include "utils/utils.hpp"

#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include "utils/logger.hpp"

namespace utils
{
	bool pathExists(const std::string &path)
	{
		struct stat info;
		return (stat(path.c_str(), &info) == 0);
	}

	bool isDirectory(const std::string &path)
	{
		struct stat info;
		if (stat(path.c_str(), &info) != 0)
			return false;
		return S_ISDIR(info.st_mode);
	}

	bool isRegularFile(const std::string &path)
	{
		struct stat info;
		if (stat(path.c_str(), &info) != 0)
			return false;
		return S_ISREG(info.st_mode);
	}

	bool isReadable(const std::string &path)
	{
		return (access(path.c_str(), R_OK) == 0);
	}

	bool isWritable(const std::string &path)
	{
		return (access(path.c_str(), W_OK) == 0);
	}

	bool isExecutable(const std::string &path)
	{
		return (access(path.c_str(), X_OK) == 0);
	}

	void trimChars(std::string &str, const std::string &chars)
	{
		size_t start = str.find_first_not_of(chars);
		size_t end = str.find_last_not_of(chars);
		if (start == std::string::npos && end == std::string::npos)
			str.clear();
		else
			str = str.substr(start, end - start + 1);
	}

	bool endsWith(const std::string &str, const std::string &suffix)
	{
		if (suffix.length() > str.length())
			return false;
		return str.substr(str.length() - suffix.length()) == suffix;
	}

	bool startsWith(const std::string &str, const std::string &prefix, const std::string &after)
	{
		if (str.length() < prefix.length())
			return false;
		if (str.compare(0, prefix.length(), prefix) != 0)
			return false;
		if (str.length() == prefix.length())
			return true;
		if (!after.empty() && after.find(str.at(prefix.length())) != std::string::npos)
			return true;
		return false;
	}

	void removeExtraChar(std::string &str, const char ch)
	{
		while (!str.empty() && str.at(str.length() - 1) == ch)
			str.erase(str.length() - 1);
	}

	std::vector<std::string> splitString(const std::string &str, const char &del)
	{
		std::vector<std::string> result;
		std::stringstream ss(str);
		std::string item;

		while (std::getline(ss, item, del))
			result.push_back(item);
		return result;
	}

	std::string buildPath(const std::string &path1, const std::string &path2)
	{
		if (path1.empty())
			return path2;
		if (path2.empty())
			return path1;

		bool path1EndsWithSlash = path1.at(path1.length() - 1) == '/';
		bool path2StartsWithSlash = path2.at(0) == '/';

		if (path1EndsWithSlash && path2StartsWithSlash)
			return path1 + path2.substr(1);
		if (!path1EndsWithSlash && !path2StartsWithSlash)
			return path1 + "/" + path2;
		return path1 + path2;
	}

	size_t hexToSizeT(const std::string &hexStr)
	{
		size_t result = 0;
		std::istringstream iss(hexStr);
		iss >> std::hex >> result;
		return result;
	}
} // namespace utils
