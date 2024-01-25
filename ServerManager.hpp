


#pragma once
#ifndef SERVERMANAGER_HPP
#define SERVERMANAGER_HPP

#include "Server.hpp"
#include "ServerConfig.hpp"
#include "MimeTypeParser.hpp"
#include "KqueueManager.hpp"

#define SERVER_TIMEOUT_CHECK_INTERVAL 5 // 5 seconds

class ServerManager
{
private:
	KqueueManager							kqueue;
	std::vector<Server *>					servers;
	std::chrono::steady_clock::time_point	lastTimeoutCheck;


public:
	static int								running;

	ServerManager(std::vector<ServerConfig> &serverConfigs, MimeTypeConfig &mimeTypes);
	~ServerManager();
	
	

	void	initializeServers(std::vector<ServerConfig> &serverConfigs, MimeTypeConfig &mimeTypes);
	void 	checkTimeouts();
	void	processReadEvent(const struct kevent &event);
	void	processWriteEvent(const struct kevent &event);

	void	start();
	void	stop();

};


#endif /* SERVERMANAGER_HPP */