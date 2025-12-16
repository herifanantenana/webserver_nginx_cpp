#pragma once

#include "config/location.hpp"
#include <string>
#include <map>
#include <vector>

namespace http
{
	class HttpRequest
	{
	public:
		enum HttpMethod
		{
			METHOD_GET,
			METHOD_POST,
			METHOD_DELETE,
			METHOD_UNKNOWN
		};

		enum ParseState
		{
			PARSE_REQUEST_LINE,
			PARSE_HEADERS,
			PARSE_BODY,
			PARSE_COMPLETE,
			PARSE_BAD_REQUEST
		};

		enum RequestType
		{
			REQ_UNKNOWN,
			REQ_STATIC,
			REQ_CGI,
			REQ_UPLOAD,
			REQ_REDIRECT
		};

	private:
		HttpMethod _method;
		std::string _uri;
		std::map<std::string, std::string> _queryString;
		std::string _httpVersion;

		// headers
		std::map<std::string, std::string> _headers;

		// body
		std::vector<char> _body;
		size_t _bodySize;
		size_t _contentLength;
		bool _isChunked;

		// parsing state
		ParseState _parseState;
		std::string _buffer;

		// request type
		RequestType _requestType;
		bool _isTypeIdentified;

		// cgi
		std::string _tempInputFile;

		// simple pupload
		std::string _uploadFilePath;
		int _uploadFd;
		bool _isSimpleUploadStreaming;
		size_t _uploadedBytes;

		// multipart upload
		bool _isMultipartUploadStreaming;
		std::string _multipartBoundary;
		std::string _multipartBuffer;
		bool _isInMultipartHeader;
		bool _isInMultipartData;
		std::string _currentMultipartFilePath;

		std::string getHeaderValue(const std::string &key) const;

		HttpMethod parseMethod(const std::string &methodStr);
		void parseQueryString(const std::string &queryStr);
		ParseState parseUri(std::string &uri);
		ParseState parseRequestLine(const std::string &line);
		ParseState parseHeaderLine(const std::string &headerLine);
		ParseState parseBodyChunk();
		ParseState parseBodyContentLength();

	public:
		HttpRequest();
		~HttpRequest();

		inline void setReqType(const RequestType &type) { _requestType = type; }

		inline const std::string &getUri() const { return _uri; }

		bool isCgiRequest(const std::vector<config::LocationConfig::CgiMapping> &cgiMappings) const;
		ParseState parse(const std::string &data, const size_t &len);
	};
} // namespace http
