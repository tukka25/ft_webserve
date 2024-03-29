


#pragma once
#ifndef KQUEUE_MANAGER_HPP
# define KQUEUE_MANAGER_HPP

#if defined(__APPLE__) || defined(__FreeBSD__)

	#include "EventPoller.hpp"
	#include "../logging/Logger.hpp"


	#include <unistd.h>
	#include <string.h>
	#include <sys/event.h>


	#define MAX_EVENTS 100

	#define KEVENT_TIMEOUT_SEC 5


	class KqueueManager : public EventPoller
	{
	public:
		KqueueManager();
		~KqueueManager();

		int				kq;
		struct kevent	events[MAX_EVENTS];

		void			registerEvent(int fd, EventType event);
		void			unregisterEvent(int fd, EventType event);
		int				waitForEvents();

		void			getNextEvent(int index, EventInfo &eventInfo);
	};

#endif

#endif /* KQUEUE_MANAGER_HPP */

