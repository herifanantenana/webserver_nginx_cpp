#include "connection/connection.hpp"

#include "config/server.hpp"
#include "http/request.hpp"

namespace connection
{
	class ClientSocket : public Connection
	{
	public:
		enum ClientState
		{
			READING_REQUEST,
			PROCESSING_REQUEST,
			EXECUTING_CGI,
			WRITING_RESPONSE,
			KEEP_ALIVE,
			CLOSING,
			CLIENT_ERROR
		};

	private:
		const config::ServerConfig &_serverConfig;
		ClientState _state;
		http::HttpRequest _request;

		// buffer
		std::vector<char> _readBuffer;
		std::string _writeBuffer;
		size_t _writeOffset;

		// keep alive
		bool _isKeepAlive;
		int _requestCount;

		// ? cgi handler

		void identifyRequestType();
		bool isCgiRequest(const config::LocationConfig *location) const;

	public:
		ClientSocket(const int fd, const config::ServerConfig &serverConfig);
		virtual ~ClientSocket();

		inline const ClientState &getState() const { return _state; }

		virtual void handleEvents(short events);
		virtual bool shouldClose() const;
		virtual void handlePollIn();
	};
} // namespace connection
