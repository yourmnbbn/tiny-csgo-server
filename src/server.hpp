#ifndef __TINY_CSGO_SERVER_HPP__
#define __TINY_CSGO_SERVER_HPP__

#ifdef _WIN32
#pragma once
#endif

#include <asio.hpp>
#include <chrono>
#include "argparser.hpp"
#include "steamauth.hpp"
#include "GCClient.hpp"
#include "bitbuf/bitbuf.h"
#include "common/proto_oob.h"
#include "common/info_const.hpp"

using namespace asio::ip;
using namespace std::chrono_literals;

inline asio::io_context g_IoContext;
inline constexpr auto CONNECTIONLESS_HEADER = -1;

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
		Steam3Server().InitServer(m_ArgParser.GetOptionValueInt16U("-port"),
			m_ArgParser.GetOptionValueString("-version"));
		Steam3Server().SetAccount(m_ArgParser.GetOptionValueString("-gslt"));
		Steam3Server().LogOn();

		if (Steam3Server().BLoggedOn())
		{
			//Connect to GC
			g_GCClient.Init();
			g_GCClient.SendHello();

			asio::co_spawn(g_IoContext, PrepareListenServer(), asio::detached);
			asio::co_spawn(g_IoContext, RunFrame(), asio::detached);
		}
	}

	void RunServer() { g_IoContext.run(); }

private:
	asio::awaitable<void> PrepareListenServer()
	{
		udp::socket socket(g_IoContext, udp::endpoint(udp::v4(), m_ArgParser.GetOptionValueInt16U("-port")));
		co_await HandleIncommingPacket(socket);
	}

	//Simulate server frame
	asio::awaitable<void> RunFrame()
	{
		while (true)
		{
			SteamGameServer_RunCallbacks();
			SendUpdatedServerDetails();
			Steam3Server().SteamGameServer()->SetAdvertiseServerActive(true);

			if (Steam3Server().GetGSSteamID().IsValid())
				UpdateGCInformation();

			asio::steady_timer timer(g_IoContext, 500ms);
			co_await timer.async_wait(asio::use_awaitable);
		}
	}

	asio::awaitable<void> HandleIncommingPacket(udp::socket& socket)
	{
		while (true)
		{
			ResetWriteBuffer();
			ResetReadBuffer();

			udp::endpoint edp;
			co_await socket.async_wait(socket.wait_read, asio::use_awaitable);
			size_t m_LastReceivedPacketLength = co_await socket.async_receive_from(asio::buffer(m_Buf), edp, asio::use_awaitable);

			printf("Receive messages from %s:%d, size %d\n", edp.address().to_string().c_str(), edp.port(), m_LastReceivedPacketLength);

			if (SteamGameServer()->HandleIncomingPacket(m_Buf, m_LastReceivedPacketLength, edp.address().to_v4().to_uint(), edp.port()))
			{
				while (true)
				{
					uint32 netadrAddress;
					uint16 netadrPort;

					auto len = SteamGameServer()->GetNextOutgoingPacket(m_Buf, sizeof(m_Buf), &netadrAddress, &netadrPort);
					if (len <= 0)
						break;

					udp::endpoint dest(make_address_v4(netadrAddress), netadrPort);
					co_await socket.async_send_to(asio::buffer(m_Buf, len), dest, asio::use_awaitable);
				}
			}
		}
	}

private:
	void SendUpdatedServerDetails()
	{
		SteamGameServer()->SetProduct("valve");
		SteamGameServer()->SetModDir(SERVER_GAME_FOLDER);
		SteamGameServer()->SetServerName(SERVER_NAME);
		SteamGameServer()->SetGameDescription(SERVER_DESCRIPTION);
		SteamGameServer()->SetGameTags(SERVER_TAG);
		SteamGameServer()->SetMapName(SERVER_MAP);
		SteamGameServer()->SetPasswordProtected(SERVER_PASSWD_NEEDED);
		SteamGameServer()->SetMaxPlayerCount(SERVER_MAX_CLIENTS);
		SteamGameServer()->SetBotPlayerCount(SERVER_NUM_FAKE_CLIENTS);
		SteamGameServer()->SetSpectatorPort(0);
	}

	void UpdateGCInformation()
	{
		CMsgGCCStrike15_v2_MatchmakingServerReservationResponse info;
		info.set_map(SERVER_MAP);

		g_GCClient.SendMessageToGC(k_EMsgGCCStrike15_v2_MatchmakingServerReservationResponse, info);
	}

	inline void		ResetWriteBuffer() { m_WriteBuf.Reset(); }
	inline void		ResetReadBuffer() { m_ReadBuf.Seek(0); }

private:
	char		m_Buf[10240];
	bf_write	m_WriteBuf;
	bf_read		m_ReadBuf;

	ArgParser&	m_ArgParser;

	uint32_t	m_LastReceivedPacketLength;
};

#endif // !__TINY_CSGO_SERVER_HPP__
