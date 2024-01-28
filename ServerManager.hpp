


#pragma once
#ifndef SERVERMANAGER_HPP
#define SERVERMANAGER_HPP

#include "Server.hpp"
#include "ServerConfig.hpp"
#include "MimeTypeParser.hpp"
#include "KqueueManager.hpp"

#define SERVER_TIMEOUT_CHECK_INTERVAL 5 // 5 seconds
// define cgi timeout interval if cgi has infinite loop or takes too long to respond
#define CGI_TIMEOUT_CHECK_INTERVAL 5 // 10 seconds

class ServerManager
{
private:
	KqueueManager							kqueue;
	std::chrono::steady_clock::time_point	lastTimeoutCheck;
	std::chrono::steady_clock::time_point	lastCgiTimeoutCheck;
	std::vector<Server *>					servers;


public:
	static int								running;

	ServerManager(std::vector<ServerConfig> &serverConfigs, MimeTypeConfig &mimeTypes);
	~ServerManager();
	
	

	void				initializeServers(std::vector<ServerConfig> &serverConfigs, MimeTypeConfig &mimeTypes);
	void 				checkTimeouts();
	void				processReadEvent(const struct kevent &event);
	void				processWriteEvent(const struct kevent &event);
	std::string			readCgiResponse(int fd);


	void	start();
	void	stop();

};


#endif /* SERVERMANAGER_HPP */
