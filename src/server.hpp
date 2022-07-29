#ifndef __TINY_CSGO_SERVER_HPP__
#define __TINY_CSGO_SERVER_HPP__

#ifdef _WIN32
#pragma once
#endif

#include <asio.hpp>
#include "argparser.hpp"
#include "steamauth.hpp"
#include "GCClient.hpp"
#include "bitbuf/bitbuf.h"

using namespace asio::ip;

inline asio::io_context g_IoContext;


class Server
{
public:
	Server(ArgParser& parser) :
		m_WriteBuf(m_Buf, sizeof(m_Buf)),
		m_ReadBuf(m_Buf, sizeof(m_Buf)),
		m_ArgParser(parser)

	{
	}

public:
	void InitializeServer()
	{
		//Connect to steam game server
		s_SteamServer.InitServer(m_ArgParser.GetOptionValueInt16U("-port"), 
			m_ArgParser.GetOptionValueString("-version"));
		s_SteamServer.SetAccount(m_ArgParser.GetOptionValueString("-gslt"));
		s_SteamServer.LogOn();

		//Connect to GC
		g_GCClient.Init();
		g_GCClient.SendHello();

		asio::co_spawn(g_IoContext, PrepareListenServer(), asio::detached);
	}

	void RunServer() { g_IoContext.run(); }

private:
	asio::awaitable<void> PrepareListenServer()
	{
		udp::socket socket(g_IoContext, udp::endpoint(udp::v4(), m_ArgParser.GetOptionValueInt16U("-port")));
		co_await HandleIncommingPacket(socket);
	}

	asio::awaitable<void> HandleIncommingPacket(udp::socket& socket)
	{
		while (true)
		{
			udp::endpoint edp;
			co_await socket.async_wait(socket.wait_read, asio::use_awaitable);
			size_t length = co_await socket.async_receive_from(asio::buffer(m_Buf), edp, asio::use_awaitable);

			printf("Receive messages from %s, size %d\n", edp.address().to_string().c_str(), length);
			//TO DO: Handle source query or other messages here
		}
	}

private:
	char		m_Buf[10240];
	bf_write	m_WriteBuf;
	bf_read		m_ReadBuf;

	ArgParser&	m_ArgParser;
};

#endif // !__TINY_CSGO_SERVER_HPP__
