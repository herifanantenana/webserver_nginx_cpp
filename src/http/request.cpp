#include "http/request.hpp"

#include "utils/logger.hpp"
#include "utils/utils.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace http
{
	HttpRequest::HttpRequest()
			: _method(METHOD_UNKNOWN),
				_uri(""),
				_httpVersion("HTTP/1.1"),
				_bodySize(0),
				_contentLength(0),
				_isChunked(false),
				_parseState(PARSE_REQUEST_LINE),
				_requestType(REQ_UNKNOWN),
				_isTypeIdentified(false),
				_uploadFd(-1),
				_isSimpleUploadStreaming(false),
				_uploadedBytes(0),
				_isMultipartUploadStreaming(false),
				_isInMultipartHeader(false),
				_isInMultipartData(false)
	{
	}

	HttpRequest::~HttpRequest()
	{
	}

	HttpRequest::HttpMethod HttpRequest::parseMethod(const std::string &methodStr)
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

	void HttpRequest::parseQueryString(const std::string &queryStr)
	{
		std::vector<std::string> pairs = utils::splitString(queryStr, '&');
		for (std::vector<std::string>::const_iterator it = pairs.begin(); it != pairs.end(); ++it)
		{
			size_t posEqual = it->find('=');
			if (posEqual != std::string::npos)
			{
				std::string key = it->substr(0, posEqual);
				std::string value = it->substr(posEqual + 1);
				std::transform(key.begin(), key.end(), key.begin(), ::tolower);
				_queryString.insert(std::make_pair(key, value));
			}
			else
				_queryString[*it] = "";
		}
	}

	HttpRequest::ParseState HttpRequest::parseUri(std::string &uri)
	{
		std::string decodedUri = "";
		while (uri.find('%') != std::string::npos)
		{
			size_t posPercent = uri.find_first_of('%');
			if (posPercent + 2 >= uri.length() ||
					!isxdigit(uri[posPercent + 1]) ||
					!isxdigit(uri[posPercent + 2]))
			{
				LOG_ERROR("Malformed percent-encoding in URI: %s", uri.c_str());
				return PARSE_BAD_REQUEST;
			}
			if (posPercent + 2 >= uri.length())
			{
				LOG_ERROR("Malformed percent-encoding in URI: %s", uri.c_str());
				return PARSE_BAD_REQUEST;
			}

			decodedUri += uri.substr(0, posPercent);
			std::string hexStr = uri.substr(posPercent + 1, 2);
			char decodedChar = static_cast<char>(std::strtol(hexStr.c_str(), NULL, 16));
			decodedUri += decodedChar;
			uri = uri.erase(0, posPercent + 3);
		}
		_uri = decodedUri;
		return _parseState;
	}

	HttpRequest::ParseState HttpRequest::parseRequestLine(const std::string &line)
	{
		std::istringstream stream(line);
		std::string methodStr;
		std::string uri;

		stream >> methodStr >> uri >> _httpVersion;
		if (methodStr.empty() || uri.empty() || _httpVersion.empty())
		{
			LOG_ERROR("Malformed request line: %s", line.c_str());
			return PARSE_BAD_REQUEST;
		}

		_method = parseMethod(methodStr);

		_parseState = parseUri(uri);
		if (_parseState == PARSE_BAD_REQUEST)
			return _parseState;
		uri = _uri;

		size_t posQuery = uri.find('?');
		if (posQuery != std::string::npos)
		{
			_uri = uri.substr(0, posQuery);
			parseQueryString(uri.substr(posQuery + 1));
		}
		else
			_uri = uri;

		return _parseState;
	}

	std::string HttpRequest::getHeaderValue(const std::string &key) const
	{
		std::map<std::string, std::string>::const_iterator it = _headers.find(key);
		if (it != _headers.end())
			return it->second;
		return "";
	}

	HttpRequest::ParseState HttpRequest::parseHeaderLine(const std::string &headerLine)
	{
		if (headerLine.empty())
		{
			LOG_DEBUG("End of headers reached");
			const std::string contentLengthStr = getHeaderValue("content-length");
			const std::string transferEncoding = getHeaderValue("transfer-encoding");

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
				return PARSE_COMPLETE;
		}
		else
		{
			size_t posColon = headerLine.find(':');
			if (posColon == std::string::npos)
			{
				LOG_ERROR("Malformed header line: %s", headerLine.c_str());
				return PARSE_BAD_REQUEST;
			}

			std::string key = headerLine.substr(0, posColon);
			std::string value = headerLine.substr(posColon + 1);

			utils::trimChars(key, " \t");
			utils::trimChars(value, " \t");

			std::transform(key.begin(), key.end(), key.begin(), ::tolower);
			_headers[key] = value;

			return _parseState;
		}
	}

	HttpRequest::ParseState HttpRequest::parseBodyChunk()
	{
		LOG_DEBUG("Parsing body chunk");
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

	HttpRequest::ParseState HttpRequest::parseBodyContentLength()
	{
		// ! not sure
		LOG_DEBUG("Parsing body with Content-Length: %zu", _contentLength);
		size_t bytesToRead = std::min(_contentLength - _body.size(), _buffer.size());
		if (bytesToRead > 0)
		{
		}
		return (_body.size() == _contentLength) ? PARSE_COMPLETE : PARSE_BODY;
	}

	HttpRequest::ParseState HttpRequest::parse(const std::string &data, const size_t &len)
	{
		_buffer.append(data.c_str(), len);
		while (_parseState != PARSE_COMPLETE && _parseState != PARSE_BAD_REQUEST)
		{
			if (_parseState == PARSE_REQUEST_LINE)
			{
				LOG_DEBUG("Parsing request line");
				size_t posEOL = _buffer.find("\r\n");
				if (posEOL == std::string::npos)
				{
					if (_buffer.size() == 0)
					{
						LOG_DEBUG("Request nothing to parse yet");
						return _parseState;
					}
					else if (_buffer.size() > 8192)
					{
						LOG_ERROR("Request line too long");
						_parseState = PARSE_BAD_REQUEST;
						return _parseState;
					}
					else
					{
						LOG_DEBUG("Request line incomplete, waiting for more data");
						return _parseState;
					}
				}

				std::string requestLine = _buffer.substr(0, posEOL);
				_buffer.erase(0, posEOL + 2);

				_parseState = parseRequestLine(requestLine);
				if (_parseState == PARSE_BAD_REQUEST)
				{
					LOG_ERROR("Failed to parse request line");
					return _parseState;
				}
				_parseState = PARSE_HEADERS;
			}
			else if (_parseState == PARSE_HEADERS)
			{
				LOG_DEBUG("Parsing headers");
				while (_parseState != PARSE_BODY && _parseState != PARSE_COMPLETE)
				{
					size_t posEOL = _buffer.find("\r\n");
					if (posEOL == std::string::npos)
					{
						if (_buffer.length() == 0)
							LOG_DEBUG("Headers nothing to parse yet");
						else if (_buffer.length() > 8192)
						{
							LOG_ERROR("Headers too long");
							_parseState = PARSE_BAD_REQUEST;
							return _parseState;
						}
						else
							LOG_DEBUG("Headers incomplete header line");
						return _parseState;
					}

					const std::string headerLine = _buffer.substr(0, posEOL);
					_buffer.erase(0, posEOL + 2);

					_parseState = parseHeaderLine(headerLine);
				}
			}
			else if (_parseState == PARSE_BODY)
			{
				LOG_DEBUG("Parsing body");
			}
		}
		return _parseState;
	}
} // namespace http
