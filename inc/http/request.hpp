#pragma once

#include "config/location.hpp"
#include <string>
#include <map>
#include <vector>

namespace http
{
	class Request
	{
	public:
		enum Method
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
			PARSE_ERROR
		};

		enum ReqType
		{
			REQ_UNKNOWN,
			REQ_STATIC,
			REQ_CGI,
			REQ_UPLOAD,
			REQ_REDIRECT
		};

	private:
		ParseState _parseState;
		// request line
		Method _method;
		std::string _uri;
		std::map<std::string, std::string> _query;
		std::string _httpVersion;
		// headers
		std::map<std::string, std::string> _headers;
		// body
		std::vector<char> _body;
		std::string _buffer;
		size_t _contentLength;
		bool _isChunked;
		// parsing helpers
		ReqType _reqType;
		bool _isTypeIdentified;
		bool _isStreamingUpload;
		int _uploadFd;
		std::string _uploadFilePath;
		size_t _uploadedBytes;

		Method parseMethod(const std::string &methodStr);
		const std::string getQueryField(const std::string &key) const;
		const std::string getHeader(const std::string &key) const;
		ParseState parseRequestLine(const std::string &line);
		ParseState parseHeadersLine(const std::string &line);

	public:
		Request();
		~Request();

		inline void setIsTypeIdentified(const bool &value) { _isTypeIdentified = value; }
		inline void setReqType(const ReqType &type) { _reqType = type; }

		inline const bool &getIsTypeIdentified() const { return _isTypeIdentified; }
		inline const ReqType &getReqType() const { return _reqType; }
		inline const std::string &getUri() const { return _uri; }

		bool isCgiRequest(const std::vector<config::LocationConfig::CgiMapping> &cgiMappings) const;
		bool isUploadRequest(const std::vector<std::string> &uploadPaths);

		bool canEnableStreamingUpload(const std::string &fileName, const std::vector<std::string> &uploadPaths);

		ParseState parse(const std::string &data, const size_t &len);
	};
} // namespace http
