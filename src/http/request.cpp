#include "http/request.hpp"

#include "utils/logger.hpp"
#include "utils/utils.hpp"
#include <sstream>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

namespace http
{
	Request::Request() : _parseState(PARSE_REQUEST_LINE), _method(Request::METHOD_UNKNOWN), _uri(""), _query(), _httpVersion(""), _headers(), _body(), _buffer(""), _bodySize(0), _contentLength(0), _isChunked(false), _reqType(REQ_UNKNOWN), _isTypeIdentified(false), _isStreamingUpload(false), _uploadFd(-1), _uploadFilePath(""), _uploadedBytes(0)
	{
	}

	Request::~Request()
	{
	}

	bool Request::canEnableStreamingUpload(const std::string &fileName, const std::vector<std::string> &uploadPaths)
	{
		if (_isStreamingUpload || _uploadFd >= 0)
		{
			LOG_WARNING("Streaming upload already enabled");
			return false;
		}

		for (std::vector<std::string>::const_iterator it = uploadPaths.begin(); it != uploadPaths.end(); ++it)
		{
			std::string path = utils::buildPath(*it, fileName);
			LOG_DEBUG("Checking upload path for streaming upload: %s", path.c_str());
			_uploadFd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (_uploadFd < 0)
			{
				LOG_WARNING("Failed to open upload file path: %s, error: %s", path.c_str(), std::strerror(errno));
				continue;
			}
			_uploadFilePath = path;
			_isStreamingUpload = true;
			return true;
		}
		LOG_DEBUG("No matching upload path found for streaming upload");
		return false;
	}

	bool Request::isCgiRequest(const std::vector<config::LocationConfig::CgiMapping> &cgiMappings) const
	{
		if (_reqType != REQ_UNKNOWN)
			return _reqType == REQ_CGI;

		for (std::vector<config::LocationConfig::CgiMapping>::const_iterator it = cgiMappings.begin(); it != cgiMappings.end(); ++it)
		{
			if (utils::endWith(_uri, it->first))
				return true;
		}
		return false;
	}

	bool Request::isUploadRequest(const std::vector<std::string> &uploadPaths)
	{
		if (_method != Request::METHOD_POST || uploadPaths.empty())
			return false;

		const std::string contentTypeStr = getHeader("content-type");
		const bool isMultipart = contentTypeStr.find("multipart/form-data") != std::string::npos;
		if (!isMultipart && !_isStreamingUpload)
		{
			if (utils::startsWith(contentTypeStr, "application/octet-stream") ||
					utils::startsWith(contentTypeStr, "image/") || utils::startsWith(contentTypeStr, "video/") || utils::startsWith(contentTypeStr, "audio/") || utils::startsWith(contentTypeStr, "text/") ||
					utils::startsWith(contentTypeStr, "application/zip") || utils::startsWith(contentTypeStr, "application/pdf"))
			{
				std::string fileNameExt = getHeader("x-filename");
				if (fileNameExt.empty())
					fileNameExt = getQueryField("filename");
				if (fileNameExt.empty())
				{
					LOG_WARNING("Upload request but no filename provided");
					fileNameExt = "upload";
				}

				std::string fileName = fileNameExt.substr(0, fileNameExt.find_last_of("."));
				std::string fileExt = fileNameExt.substr(fileNameExt.find_last_of(".") + 1);
				char timestamp[fileName.length() + 25];
				snprintf(timestamp, sizeof(timestamp), "%s-%ld.%s", fileName.c_str(), static_cast<long>(time(NULL)), fileExt.c_str());
				fileName = std::string(timestamp);
				LOG_CONSOLE("Inferred upload filename: %s", fileName.c_str());
				if (canEnableStreamingUpload(fileName, uploadPaths))
				{
					LOG_DEBUG("Enabling streaming upload for file: %s", fileName.c_str());
					return true;
				}
				else
				{
					LOG_DEBUG("Streaming upload not enabled for file: %s", fileName.c_str());
					return false;
				}
			}
			else
			{
				LOG_DEBUG("Not an upload request based on Content-Type: %s", contentTypeStr.c_str());
				return false;
			}
		}
		if (isMultipart)
		{
			LOG_DEBUG("Multipart upload request detected");
		}
		return false;
	}

	Request::Method Request::parseMethod(const std::string &methodStr)
	{
		if (methodStr == "GET")
			return METHOD_GET;
		else if (methodStr == "POST")
			return METHOD_POST;
		else if (methodStr == "DELETE")
			return METHOD_DELETE;
		else
			return METHOD_UNKNOWN;
	}

	const std::string Request::getQueryField(const std::string &key) const
	{
		std::string lowerKey = key;
		std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
		std::map<std::string, std::string>::const_iterator it = _query.find(lowerKey);
		if (it != _query.end())
			return it->second;
		return "";
	}
	const std::string Request::getHeader(const std::string &key) const
	{
		std::string lowerKey = key;
		std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), tolower);
		std::map<std::string, std::string>::const_iterator it = _headers.find(lowerKey);
		if (it != _headers.end())
			return it->second;
		return "";
	}

	Request::ParseState Request::parseRequestLine(const std::string &line)
	{
		std::istringstream stream(line);
		std::string methodStr;
		std::string uri;
		std::string version;

		stream >> methodStr >> uri >> version;
		if (methodStr.empty() || uri.empty() || version.empty())
		{
			// todo: continue parser
			LOG_ERROR("Malformed request line");
			return PARSE_ERROR;
		}

		_method = parseMethod(methodStr);

		size_t posQuery = uri.find('?');
		if (posQuery != std::string::npos)
		{
			_uri = uri.substr(0, posQuery);
			std::string queryStr = uri.substr(posQuery + 1);
			std::vector<std::string> pairs = utils::splitString(queryStr, '&');
			for (std::vector<std::string>::iterator it = pairs.begin(); it != pairs.end(); ++it)
			{
				size_t posEqual = it->find('=');
				if (posEqual != std::string::npos)
				{
					std::string key = it->substr(0, posEqual);
					std::string value = it->substr(posEqual + 1);
					std::transform(key.begin(), key.end(), key.begin(), ::tolower);
					_query.insert(std::make_pair(key, value));
				}
				else
				{
					_query.insert(std::make_pair(*it, ""));
				}
			}
		}
		else
		{
			_uri = uri;
		}
		_httpVersion = version;

		return PARSE_HEADERS;
	}

	Request::ParseState Request::parseHeadersLine(const std::string &line)
	{
		if (line.empty())
		{
			const std::string contentLengthStr = getHeader("Content-Length");
			const std::string transferEncoding = getHeader("Transfer-Encoding");
			if (!contentLengthStr.empty())
			{
				_contentLength = std::atol(contentLengthStr.c_str());
				return (_contentLength > 0) ? PARSE_BODY : PARSE_COMPLETE;
			}
			else if (transferEncoding.find("chunked") != std::string::npos)
			{
				_isChunked = true;
				return PARSE_BODY;
			}
			else
			{
				return PARSE_COMPLETE;
			}
		}
		else
		{
			size_t posColon = line.find(':');
			if (posColon == std::string::npos)
			{
				LOG_ERROR("Malformed header line");
				return PARSE_ERROR;
			}

			std::string key = line.substr(0, posColon);
			std::string value = line.substr(posColon + 1);

			utils::trimChars(key);
			utils::trimChars(value);

			std::transform(key.begin(), key.end(), key.begin(), ::tolower);

			_headers.insert(std::make_pair(key, value));
			// ? display parsed header line
			LOG_CONSOLE("Parsed header line: key=%s, value=%s", key.c_str(), value.c_str());
			return PARSE_HEADERS;
		}
	}

	Request::ParseState Request::parseBodyChunk()
	{
		LOG_DEBUG("Parsing chunked body");

		size_t chunkLineSize = 0;
		do
		{
			size_t posEndSize = _buffer.find("\r\n");
			if (posEndSize == std::string::npos)
				return PARSE_BODY;
			std::string lineSize = _buffer.substr(0, posEndSize);
			size_t posEndValue = _buffer.find("\r\n", posEndSize + 2);
			if (posEndValue == std::string::npos)
				return PARSE_BODY;
			std::string lineValue = _buffer.substr(posEndSize + 2, posEndValue);
			chunkLineSize = utils::hexToSizeT(lineSize);
			if (chunkLineSize == 0)
				return PARSE_COMPLETE;
			if (lineValue.length() != chunkLineSize)
				return PARSE_BODY;
			_buffer.erase(0, posEndValue + 2);
			_body.insert(_body.end(), lineValue.begin(), lineValue.end());
		} while (_buffer.length() > 0);
		return PARSE_BODY;
	}

	Request::ParseState Request::parseBodyContentLength()
	{
		LOG_DEBUG("Parsing body with Content-Length: %zu", _contentLength);
		size_t toRead = std::min(_contentLength - _bodySize, _buffer.length());
		if (toRead > 0)
		{
			// LOG_DEBUG("is Streaming Upload: %d, Upload FD: %d", _isStreamingUpload, _uploadFd);
			if (_isStreamingUpload && _uploadFd >= 0)
			{
				LOG_DEBUG("Streaming upload: writing %zu bytes to file: %s", toRead, _uploadFilePath.c_str());
				ssize_t byteWritten = write(_uploadFd, _buffer.data(), toRead);
				if (byteWritten < 0)
				{
					LOG_ERROR("Error writing to upload file: %s", _uploadFilePath.c_str());
					return PARSE_ERROR;
				}
				_uploadedBytes += static_cast<size_t>(byteWritten);
				_bodySize += static_cast<size_t>(byteWritten);
				_buffer.erase(0, byteWritten);
			}
			else
			{
				LOG_DEBUG("Reading %zu bytes into body buffer", toRead);
				_body.insert(_body.end(), _buffer.begin(), _buffer.begin() + toRead);
				_bodySize += toRead;
				_buffer.erase(0, toRead);
			}
		}

		if (_bodySize >= _contentLength)
		{
			if (_isStreamingUpload && _uploadFd >= 0)
			{
				close(_uploadFd);
				_uploadFd = -1;
				LOG_CONSOLE("Completed streaming upload to file: %s, total bytes: %zu", _uploadFilePath.c_str(), _uploadedBytes);
			}
			return PARSE_COMPLETE;
		}
		return PARSE_BODY;
	}

	Request::ParseState Request::parse(const std::string &data, const size_t &len)
	{
		LOG_DEBUG("Request parse called with data length: %zu", len);
		_buffer.append(data.c_str(), len);
		while (_parseState != PARSE_COMPLETE && _parseState != PARSE_ERROR)
		{
			if (_parseState == Request::PARSE_REQUEST_LINE)
			{
				LOG_DEBUG("Parsing request line");
				size_t posEnd = _buffer.find("\r\n");
				if (posEnd == std::string::npos)
				{
					if (_buffer.length() == 0)
						LOG_DEBUG("Request nothing to parse yet");
					else if (_buffer.length() > 4096)
					{
						LOG_ERROR("Request line too long");
						_parseState = PARSE_ERROR;
						return _parseState;
					}
					else
						LOG_DEBUG("Request incomplete request line");
					return _parseState;
				}

				const std::string line = _buffer.substr(0, posEnd);
				_buffer.erase(0, posEnd + 2);

				_parseState = parseRequestLine(line);
				// ? display parsed request line
				if (_parseState != PARSE_ERROR)
					LOG_CONSOLE("Parsed request line: method=%d, uri=%s, version=%s", _method, _uri.c_str(), _httpVersion.c_str());
			}
			else if (_parseState == Request::PARSE_HEADERS)
			{
				LOG_DEBUG("Parsing headers");
				while (_parseState != Request::PARSE_BODY && _parseState != Request::PARSE_COMPLETE)
				{
					size_t posEnd = _buffer.find("\r\n");
					if (posEnd == std::string::npos)
					{
						if (_buffer.length() == 0)
							LOG_DEBUG("Headers nothing to parse yet");
						else if (_buffer.length() > 8192)
						{
							LOG_ERROR("Headers too long");
							_parseState = PARSE_ERROR;
							return _parseState;
						}
						else
							LOG_DEBUG("Headers incomplete header line");
						return _parseState;
					}

					const std::string line = _buffer.substr(0, posEnd);
					_buffer.erase(0, posEnd + 2);

					_parseState = parseHeadersLine(line);
				}
			}
			else if (_parseState == Request::PARSE_BODY)
			{
				LOG_DEBUG("Parsing body");
				_parseState = _isChunked ? parseBodyChunk() : parseBodyContentLength();
			}
		}
		return _parseState;
	}
} // namespace http
