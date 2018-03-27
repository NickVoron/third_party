#pragma once

#include "atomicNetCore/library.include.h"
#include "common/waitable_queue.h"

#include "utp.h"

#include <memory>
#include <set>
#include <list>
#include <vector>


namespace utp
{
	enum class SocketState : uint8_t
	{
		CONNECTED,
		CONNECTING,
		ACCEPTED
	};

	struct socket_data
	{
		socket_data(SocketState state) : state(state) {}

		SocketState state;
		std::list<std::vector<byte>> messages;
	};

	struct core
	{
		core(uint16_t listenPort);
		virtual ~core();

		utp_socket* connect(const net::address4& addr);

		void update_udp();
		
		mt::waitable_queue<utp_socket*> incoming;
		utp_context* context = nullptr;
		net::socket_type fd = INVALID_SOCKET;
	};

	void printSocketStats(utp_socket* sock);
}

