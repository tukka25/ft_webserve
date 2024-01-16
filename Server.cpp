#include "Server.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <string>
#include <sys/_types/_size_t.h>
#include <sys/event.h>
#include <unistd.h>

Server::Server(ServerConfig &config, MimeTypeParser &mimeTypes, KqueueManager &kq) : _config(config), _mimeTypes(mimeTypes), _kq(kq)
{
	// std::cout << "Server constructor" << std::endl;
	_serverAddr.sin_family = AF_INET;
	_serverAddr.sin_port = htons(_config.port);
	_serverAddr.sin_addr.s_addr = inet_addr(_config.ipAddress.c_str());
	memset(_serverAddr.sin_zero, '\0', sizeof(_serverAddr.sin_zero));
}

Server::~Server()
{
	std::cout << "Server destructor" << std::endl;
	std::map<int, ClientState *>::iterator it = _clients.begin();
	while (it != _clients.end())
	{
		_kq.unregisterEvent(it->first, EVFILT_READ);
		close(it->first);
		it++;
	}
	_clients.clear();
	_kq.unregisterEvent(_socket, EVFILT_READ);
	close(_socket);
}

void	Server::createServerSocket()
{
	this->_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (this->_socket < 0)
		throw std::runtime_error("Error: socket creation failed");
}

void	Server::setSocketOptions()
{
	int opt = 1;
	if (setsockopt(this->_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		throw std::runtime_error("Error: setsockopt failed");
}


void	Server::setSocketToNonBlocking()
{
	int flags = fcntl(this->_socket, F_GETFL, 0);
	if (flags < 0)
		throw std::runtime_error("Error: fcntl failed");
	if (fcntl(this->_socket, F_SETFL, flags | O_NONBLOCK) < 0)
		throw std::runtime_error("Error: fcntl failed");
}

void	Server::bindAndListen()
{
	if (bind(this->_socket, (struct sockaddr *)&this->_serverAddr, sizeof(this->_serverAddr)) < 0)
		throw std::runtime_error("Error: bind failed");
	if (listen(this->_socket, SERVER_BACKLOG) < 0)
		throw std::runtime_error("Error: listen failed");
}


void	Server::acceptNewConnection()
{
	std::cout << "Accepting new connection" << std::endl;
	struct sockaddr_in	clientAddr;
	socklen_t			clientAddrLen = sizeof(clientAddr);
	int clientSocket = accept(this->_socket, (struct sockaddr *)&clientAddr, &clientAddrLen);
	if (clientSocket < 0)
		throw std::runtime_error("Error: accept failed");
	ClientState *clientState = new ClientState(clientSocket);
	_clients[clientSocket] = clientState;
	_kq.registerEvent(clientSocket, EVFILT_READ);
}

void	Server::handleClientDisconnection(int clientSocket)
{
	std::cout << "Handling client disconnection" << std::endl;
	_kq.unregisterEvent(clientSocket, EVFILT_READ);\
	ClientState *clientState = _clients[clientSocket];
	_clients.erase(clientSocket);
	delete clientState;
	if (_responses.count(clientSocket) > 0)
	{
		_kq.unregisterEvent(clientSocket, EVFILT_WRITE);
		ResponseState *responseState = _responses[clientSocket];
		_responses.erase(clientSocket);
		delete responseState;
	}
	close(clientSocket);
}

void	Server::handleClientRequest(int clientSocket)
{
	char buffer[BUFFER_SIZE];
	int bytesRead = recv(clientSocket, buffer, BUFFER_SIZE, 0);
	if (bytesRead < 0)
	{
		// handle recv error
		removeClient(clientSocket);
		close(clientSocket);
	}

	if (bytesRead > 0)
	{
		ClientState *client = _clients[clientSocket];

		client->updateLastRequestTime();
		client->incrementRequestCount();


	}
}

void	ClientState::processIncomingData(Server &server, const char *buffer, int bytesRead)
{
	if (!areHeaderComplete)
	{
		requestHeaders.append(buffer, bytesRead);
		if (requestHeaders.size() > MAX_REQUEST_HEADERS_SIZE)
		{
			server.handleHeaderSizeExceeded(fd);
			return;
		}
		if (headersCompleted(buffer))
		{
			areHeaderComplete = true;

			// seperate headers and body
			size_t endOfHeaders = requestHeaders.find("\r\n\r\n") + 4;
			requestBody = requestHeaders.substr(endOfHeaders);
			requestHeaders.resize(endOfHeaders);

			this->request = new HttpRequest(requestHeaders);

			if (request->getUri().size() > MAX_URI_SIZE)
			{
				server.handleUriTooLarge(fd);
				return;
			}

			if (request->getMethod() == "GET")
			{
				if (!requestBody.empty() || request->getHeader("Content-Length") != "" || request->getHeader("Transfer-Encoding") == "chunked")
				{
					server.handleInvalidGetRequest(fd);
					return;
				}
				server.processGetRequest(fd, *request);
				return;
			}

			else if (request->getMethod() == "POST")
			{
				if (request->getStatus() != 200)
				{
					server.handleInvalidPostRequest(fd, request->getStatus());
					return;
				}
			}
		}

	}
}

void	Server::processGetRequest(int clientSocket, HttpRequest &request)
{
	ResponseState *responseState;

	RequestHandler handler(_config, _mimeTypes);
	HttpResponse response = handler.handleRequest(request);

	
	if (response.getType() == SMALL_FILE)
		responseState = new ResponseState(response.buildResponse());
	else 
		responseState = new ResponseState(response.buildResponseHeaders(), response.filePath, response.fileSize);
	
	_responses[clientSocket] = responseState;
	_kq.registerEvent(clientSocket, EVFILT_WRITE);
}

void	Server::handleHeaderSizeExceeded(int clientSocket)
{
	HttpResponse response;

	removeClient(clientSocket);
	response.generateStandardErrorResponse("400", "Bad Request", "400 Request Header Or Cookie Too Large", "Request Header Or Cookie Too Large");
	ResponseState *responseState = new ResponseState(response.buildResponse(), true);
	_responses[clientSocket] = responseState;
	_kq.registerEvent(clientSocket, EVFILT_WRITE);
}

void	Server::handleUriTooLarge(int clientSocket)
{
	HttpResponse response;

	removeClient(clientSocket);
	response.generateStandardErrorResponse("414", "Request-URI Too Large", "414 Request-URI Too Large");
	ResponseState *responseState = new ResponseState(response.buildResponse(), true);
	_responses[clientSocket] = responseState;
	_kq.registerEvent(clientSocket, EVFILT_WRITE);
}

void	Server::handleInvalidGetRequest(int clientSocket)
{
	HttpResponse response;

	removeClient(clientSocket);
	response.generateStandardErrorResponse("400", "Bad Request", "400 Invalid GET Request (with body indicators)", "Invalid GET Request (with body indicators)");
	ResponseState *responseState = new ResponseState(response.buildResponse(), true);
	_responses[clientSocket] = responseState;
	_kq.registerEvent(clientSocket, EVFILT_WRITE);
}



void	Server::handleInvalidPostRequest(int clientSocket, int requestStatusCode)
{
	std::string statusCode = std::to_string(requestStatusCode);
	std::string statusMessage = getStatusMessage(requestStatusCode);

	HttpResponse response;
	removeClient(clientSocket);
	response.generateStandardErrorResponse(statusCode, statusMessage, statusCode + " " + statusMessage);
	ResponseState *responseState = new ResponseState(response.buildResponse(), true);
	_responses[clientSocket] = responseState;
	_kq.registerEvent(clientSocket, EVFILT_WRITE);
}

void	Server::handleClientResponse(int clientSocket)
{
	if (_responses.count(clientSocket) == 0)
	{
		std::cerr << "Error: no response state found for client socket " << clientSocket << std::endl;
		return;
	}
	std::cout << "Handling client response" << std::endl;
	ResponseState *responseState = _responses[clientSocket];

	if (responseState->getType() == SMALL_FILE)
	{
		std::cout << "Sending small file" << std::endl;
		const std::string &response = responseState->getSmallFileResponse();
		int bytesSent = send(clientSocket, response.c_str(), response.length(), 0);
		if (bytesSent < 0)
			throw std::runtime_error("Error: send failed");
		if (bytesSent == (int)response.length())
		{
			_kq.unregisterEvent(clientSocket, EVFILT_WRITE);
			_responses.erase(clientSocket);
			delete responseState;
		}
	}
	else if (responseState->getType() == LARGE_FILE)
	{
		// send headers first, then send file in chunks
		if (!responseState->isHeaderSent)
		{
			std::cout << "Sending headers ..." << std::endl;
			const std::string &headers = responseState->getHeaders();
			int bytesSent = send(clientSocket, headers.c_str(), headers.length(), 0);
			if (bytesSent < 0)
			{
				if (errno == EPIPE)
				{
					std::cerr << "Client disconnected (Broken Pipe)" << std::endl;
				}
				else
				{
					std::cerr << "Error: send failed, errno: " << errno << std::endl;
				}
				_kq.unregisterEvent(clientSocket, EVFILT_WRITE);
				_responses.erase(clientSocket);
				delete responseState;
				close(clientSocket);
			}
			if (bytesSent == (int)headers.length())
				responseState->isHeaderSent = true;
		}
		else
		{
			std::cout << "Sending chunks ..." << std::endl;
			// send file in chunks
			std::string chunk = responseState->getNextChunk();

			std::cout << "start sending ..." << std::endl;

			// convert chunk size to hex
			std::stringstream ss;
			ss << std::hex << chunk.length();
			std::string chunkSizeHex = ss.str();

			std::string formattedChunk = chunkSizeHex + "\r\n" + chunk + "\r\n";
			std::cout << "before send" << std::endl;
			int bytesSent = send(clientSocket, formattedChunk.c_str(), formattedChunk.length(), 0);
			std::cout << "after send" << std::endl;
			if (bytesSent < 0)
			{
				if (errno == EPIPE)
				{
					std::cerr << "Client disconnected (Broken Pipe)" << std::endl;
				}
				else
				{
					std::cerr << "Error: send failed, errno: " << errno << std::endl;
				}
				_kq.unregisterEvent(clientSocket, EVFILT_WRITE);
				_responses.erase(clientSocket);
				delete responseState;
				close(clientSocket);
			}
			if (bytesSent == (int)formattedChunk.length())
			{
				std::cout << "Chunk sent completely !" << std::endl;
				if (responseState->isFinished())
				{
					std::cout << "Sending end chunk" << std::endl;
					std::string endChunk = "0\r\n\r\n";
					int bytesSent = send(clientSocket, endChunk.c_str(), endChunk.length(), 0);
					if (bytesSent < 0)
					{
						if (errno == EPIPE)
						{
							std::cerr << "Client disconnected (Broken Pipe)" << std::endl;
						}
						else
						{
							std::cerr << "Error: send failed, errno: " << errno << std::endl;
						}
						_kq.unregisterEvent(clientSocket, EVFILT_WRITE);
						_responses.erase(clientSocket);
						delete responseState;
						close(clientSocket);
					}
					_kq.unregisterEvent(clientSocket, EVFILT_WRITE);
					_responses.erase(clientSocket);
					delete responseState;
				}
			}
			else {
				std::cout << "Chunk not sent completely !" << std::endl;
			}
		}
	}
}

void	Server::checkForTimeouts()
{
	std::map<int, ClientState *>::iterator it = _clients.begin();
	while (it != _clients.end())
	{
		if (it->second->isTimedOut(this->_config.keepalive_timeout))
		{
			std::cerr << "Client timed out" << std::endl;
			_kq.unregisterEvent(it->first, EVFILT_READ);
			close(it->first);
			delete it->second;
			it = _clients.erase(it);
		}
		else
			it++;
	}
}

void	Server::removeClient(int clientSocket)
{
	ClientState *clientState = _clients[clientSocket];
	_kq.unregisterEvent(clientSocket, EVFILT_READ);
	_clients.erase(clientSocket);
	delete clientState;
}

std::string	Server::getStatusMessage(int statusCode)
{
	std::map<int, std::string>			statusCodeMessages;

	statusCodeMessages[400] = "Bad Request";
	statusCodeMessages[411] = "Length Required";
	statusCodeMessages[413] = "Request Entity Too Large";
	statusCodeMessages[414] = "Request-URI Too Large";
	statusCodeMessages[500] = "Internal Server Error";
	statusCodeMessages[501] = "Not Implemented";
	statusCodeMessages[505] = "HTTP Version Not Supported";

	if (statusCodeMessages.count(statusCode) == 0)
		return "";
	return statusCodeMessages[statusCode];
}


void	Server::run()
{
	createServerSocket();
	setSocketOptions();
	setSocketToNonBlocking();
	bindAndListen();
	_kq.registerEvent(_socket, EVFILT_READ);
	_running = true;
}

void	Server::stop()
{
	this->_running = false;
	std::map<int, ClientState *>::iterator it = _clients.begin();
	while (it != _clients.end())
	{
		_kq.unregisterEvent(it->first, EVFILT_READ);
		close(it->first);
		it++;
	}
	_clients.clear();
}


// #=================# ClientState #=================#

ClientState::ClientState(int fd)
	: fd(fd), lastRequestTime(std::chrono::steady_clock::now()), requestCount(0),
	areHeaderComplete(false), isBodyComplete(false), request(NULL) { }

void	ClientState::updateLastRequestTime()
{
	lastRequestTime = std::chrono::steady_clock::now();
}

void	ClientState::incrementRequestCount()
{
	requestCount++;
}

int		ClientState::getFd() const
{
	return fd;
}

bool	ClientState::isTimedOut(size_t keepalive_timeout) const
{
	std::chrono::seconds timeoutDuration(keepalive_timeout);
	std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
	return (std::chrono::duration_cast<std::chrono::seconds>(now - lastRequestTime) > timeoutDuration);
}

bool	ClientState::headersCompleted(const char *buffer) const
{
	return (strstr(buffer, "\r\n\r\n") != NULL);
}