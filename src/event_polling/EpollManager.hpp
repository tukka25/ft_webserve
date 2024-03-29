

#pragma once
#ifndef EPOLL_MANAGER_HPP
#define EPOLL_MANAGER_HPP

#ifdef __linux__

	#include "EventPoller.hpp"
	#include "../logging/Logger.hpp"

	#include <set>
	#include <unistd.h>
	#include <string.h>
	#include <sys/epoll.h>

	#define MAX_EVENTS 100

	#define EPOLL_TIMEOUT 5000

	class EpollManager : public EventPoller
	{
	public:
		EpollManager();
		~EpollManager();

		int					epfd;
		struct epoll_event	events[MAX_EVENTS];
		std::set<int>		registeredEvents;

		void			registerEvent(int fd, EventType event);
		void			unregisterEvent(int fd, EventType event);
		int				waitForEvents();

		void			getNextEvent(int index, EventInfo &eventInfo);
	};

#endif


#endif /* EPOLL_MANAGER_HPP */
