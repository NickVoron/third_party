#include "libutp.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include "stuff/enforce.h"


namespace utp
{
	int o_debug = UTP_DEBUG_LOGGING;
	int o_listen;
	int eof_flag;

	void hexdump(const void *vp, size_t len)
	{
		int count = 1;
		const char *p = (const char*) vp;

		while (len--)
		{
			if (count == 1)
				fprintf(stderr, "    %p: ", p);

			fprintf(stderr, " %02x", *p & 0xff);
			++p;

			if (count++ == 16) {
				fprintf(stderr, "\n");
				count = 1;
			}
		}

		if (count != 1)
			fprintf(stderr, "\n");
	}
	
	uint64 callback_on_read(utp_callback_arguments *a)
	{
		core* kernel = static_cast<core*>(utp_context_get_userdata(a->context));
		
		socket_data* userData = (socket_data*)utp_get_userdata(a->socket);
		if (userData->state == SocketState::ACCEPTED)
		{
			userData->state = SocketState::CONNECTED;
			kernel->incoming.send(a->socket);
		}
		else
		{
			userData->messages.emplace_back(&a->buf[0], &a->buf[0] + a->len);
		}
		
		utp_read_drained(a->socket);
				
		return 0;
	}

	uint64 callback_on_firewall(utp_callback_arguments* a)
	{
		core* kernel = static_cast<core*>(utp_context_get_userdata(a->context));

		// 		if (!o_listen) 
		//		{
		// 			LOG_MSG("Firewalling unexpected inbound connection in non-listen mode");
		// 			return 1;
		// 		}
		
// 		if (kernel->accepted.count(a->socket))
// 		{
// 			LOG_MSG("Firewalling unexpected second inbound connection");
// 			return 1;
// 		}

		LOG_MSG("Firewall allowing inbound connection");
		return 0;
	}

	uint64 callback_on_accept(utp_callback_arguments* a)
	{
		core* kernel = static_cast<core*>(utp_context_get_userdata(a->context));
		LOG_MSG("accepted inbound socket " << a->socket);
		utp_set_userdata(a->socket, new socket_data(SocketState::ACCEPTED));
		return 0;
	}

	uint64 callback_on_connect(utp_callback_arguments* a)
	{
		core* kernel = static_cast<core*>(utp_context_get_userdata(a->context));
		utp_set_userdata(a->socket, new socket_data(SocketState::CONNECTING));
 		int v = 0; utp_write(a->socket, &v, sizeof(v));
		return 0;
	}

	uint64 callback_on_error(utp_callback_arguments *a)
	{
		LOG_ERROR("error: " << utp_error_code_names[a->error_code]);
		utp_close(a->socket);
		return 0;
	}

	void printSocketStats(utp_socket* sock)
	{
		if (auto stats = utp_get_stats(sock))
		{
			LOG_MSG("Socket Statistics:");
			LOG_MSG("    Bytes sent:          " << stats->nbytes_xmit);
			LOG_MSG("    Bytes received:      " << stats->nbytes_recv);
			LOG_MSG("    Packets received:    " << stats->nrecv);
			LOG_MSG("    Packets sent:        " << stats->nxmit);
			LOG_MSG("    Duplicate receives:  " << stats->nduprecv);
			LOG_MSG("    Retransmits:         " << stats->rexmit);
			LOG_MSG("    Fast Retransmits:    " << stats->fastrexmit);
			LOG_MSG("    Best guess at MTU:   " << stats->mtu_guess);
		}
		else
		{
			LOG_MSG("No socket statistics available");
		}
	}

	uint64 callback_on_state_change(utp_callback_arguments *a)
	{
		LOG_MSG("state " << a->state << " " << utp_state_names[a->state]);

		core* kernel = static_cast<core*>(utp_context_get_userdata(a->context));

		switch (a->state)
		{
		case UTP_STATE_CONNECT:
		case UTP_STATE_WRITABLE:
			break;

		case UTP_STATE_EOF:
			LOG_MSG("received EOF from socket; closing");
			utp_close(a->socket);
			break;

		case UTP_STATE_DESTROYING:
			LOG_MSG("UTP socket is being destroyed; exiting");
			printSocketStats(a->socket);
			break;
		}

		return 0;
	}

	uint64 callback_sendto(utp_callback_arguments *a)
	{
		core* kernel = static_cast<core*>(utp_context_get_userdata(a->context));

		struct sockaddr_in* sin = (struct sockaddr_in*)a->address;
		ENFORCE(sin);

//		LOG_MSG("sendto: " << a->len << " byte packet to " << net::address4(*sin) << ((a->flags & UTP_UDP_DONTFRAG) ? "  (DF bit requested, but not yet implemented)" : ""));

		if (o_debug >= 3)
		{
			hexdump(a->buf, a->len);
		}

		sendto(kernel->fd, (const char*) a->buf, a->len, 0, a->address, a->address_len);
		return 0;
	}

	uint64 callback_log(utp_callback_arguments *a)
	{
		LOG_ERROR("log: " << a->buf);
		return 0;
	}
	

	core::core(uint16_t listenPort)
	{
		fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (fd >= 0)
		{
#ifdef WIN32
			u_long mode = 1;
			ioctlsocket(fd, FIONBIO, &mode);
#else
			fcntl(fd, F_SETFL, O_NONBLOCK);
#endif

			net::host_info hostInfo;
			auto lanaddr = hostInfo.getLAN();
			if (lanaddr.first)
			{
				auto addr = net::address4(lanaddr.second, listenPort);
				auto bindres = bind(fd, (struct sockaddr*)&addr.address(), sizeof(addr.address()));
				if (bindres == 0)
				{
					struct sockaddr_in sin;
					socklen_t len = sizeof(sin);
					if (getsockname(fd, (struct sockaddr *) &sin, &len) == 0)
					{
						context = utp_init(2);
						ENFORCE(context);

						utp_context_set_userdata(context, this);
						utp_set_callback(context, UTP_LOG, &callback_log);
						utp_set_callback(context, UTP_SENDTO, &callback_sendto);
						utp_set_callback(context, UTP_ON_ERROR, &callback_on_error);
						utp_set_callback(context, UTP_ON_STATE_CHANGE, &callback_on_state_change);
						utp_set_callback(context, UTP_ON_READ, &callback_on_read);
						utp_set_callback(context, UTP_ON_FIREWALL, &callback_on_firewall);
						utp_set_callback(context, UTP_ON_ACCEPT, &callback_on_accept);
						utp_set_callback(context, UTP_ON_CONNECT, &callback_on_connect);

						if (o_debug >= 2)
						{
							utp_context_set_option(context, UTP_LOG_NORMAL, 1);
							utp_context_set_option(context, UTP_LOG_MTU, 1);
							utp_context_set_option(context, UTP_LOG_DEBUG, 1);
						}

						// LOG_MSG("UTP listening on local: " << net::address4(sin));
					}
				}
			}
			else
			{
				//LOG_ERROR("getaddrinfo: " << gai_strerror(error));
			}
		}
	}

	core::~core()
	{
		if (context)
		{
			utp_context_stats *stats = utp_get_context_stats(context);

			if (stats)
			{
				// 			LOG_MSG("           Bucket size:    <23    <373    <723    <1400    >1400\n");
				// 			LOG_MSG("Number of packets sent:  %5d   %5d   %5d    %5d    %5d\n",	stats->_nraw_send[0], stats->_nraw_send[1], stats->_nraw_send[2], stats->_nraw_send[3], stats->_nraw_send[4]);
				// 			LOG_MSG("Number of packets recv:  %5d   %5d   %5d    %5d    %5d\n",	stats->_nraw_recv[0], stats->_nraw_recv[1], stats->_nraw_recv[2], stats->_nraw_recv[3], stats->_nraw_recv[4]);
			}
			else
			{
				LOG_MSG("utp_get_context_stats() failed?");
			}
#ifdef WIN32
			::closesocket(fd);
#else
			::close(fd);
#endif // WIN32


			LOG_MSG("destorying context");
			utp_destroy(context);
		}		
	}

	utp_socket* core::connect(const net::address4& addr)
	{
		ENFORCE(context);
		auto sock = utp_create_socket(context);
		ENFORCE(sock);
		struct addrinfo hints, *res = nullptr;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		hints.ai_flags = AI_NUMERICHOST;

		if (int error = getaddrinfo(addr.addr_str().c_str(), addr.port_str().c_str(), &hints, &res))
		{
			LOG_MSG("utp::core::connect: incorrect network address: " << addr);
			utp_close(sock);
			sock = nullptr;
		}
		else
		{
			LOG_MSG("connecting to " << net::address4(*res));
			utp_connect(sock, res->ai_addr, res->ai_addrlen);
		}		

		freeaddrinfo(res);

		return sock;
	}

	void core::update_udp()
	{
		if (context)
		{
			char socket_data[4096];
			struct sockaddr_in src_addr;
			socklen_t addrlen = sizeof(src_addr);

			ssize_t len = recvfrom(fd, socket_data, sizeof(socket_data), 0, (struct sockaddr *)&src_addr, &addrlen);

			if (len > 0)
			{
				if (!utp_process_udp(context, (byte*) socket_data, len, (struct sockaddr *)&src_addr, addrlen))
				{
					LOG_MSG("UDP packet not handled by UTP:  ignoring");
				}
			}

			utp_issue_deferred_acks(context);
		}		
	}
}
