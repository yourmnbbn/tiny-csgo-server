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

			if (!(co_await ProcessConnectionlessPacket(socket, edp, m_ReadBuf)))
			{
				if (!SteamGameServer()->HandleIncomingPacket(m_Buf, m_LastReceivedPacketLength, edp.address().to_v4().to_uint(), edp.port()))
					co_return;

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

	asio::awaitable<bool> ProcessConnectionlessPacket(udp::socket& socket, udp::endpoint remote_endpoint, bf_read& msg)
	{
		if (msg.ReadLong() != CONNECTIONLESS_HEADER)
			co_return false;

		int c = msg.ReadByte();
		switch (c)
		{
		case A2S_INFO:
		{
			if (CONFIG_HANDLE_QUERY_BY_STEAM)
				co_return false;

			char key[24];
			msg.ReadString(key, sizeof(key));
			if (strcmp(key, A2S_KEY_STRING))
				co_return false;

			m_WriteBuf.WriteLong(CONNECTIONLESS_HEADER);
			m_WriteBuf.WriteByte(S2A_INFO_SRC);

			m_WriteBuf.WriteByte(17); //protocol version
			m_WriteBuf.WriteString(SERVER_NAME);
			m_WriteBuf.WriteString(SERVER_MAP);
			m_WriteBuf.WriteString(SERVER_GAME_FOLDER);
			m_WriteBuf.WriteString(SERVER_DESCRIPTION);

			m_WriteBuf.WriteShort(SERVER_APPID);
			m_WriteBuf.WriteByte(SERVER_NUM_CLIENTS);
			m_WriteBuf.WriteByte(SERVER_MAX_CLIENTS);
			m_WriteBuf.WriteByte(SERVER_NUM_FAKE_CLIENTS);
			m_WriteBuf.WriteByte(SERVER_TYPE);
			m_WriteBuf.WriteByte(SERVER_OS);

			m_WriteBuf.WriteByte(SERVER_PASSWD_NEEDED);
			m_WriteBuf.WriteByte(SERVER_VAC_STATES);
			m_WriteBuf.WriteString(m_ArgParser.GetOptionValueString("-version"));

			//EDF
			m_WriteBuf.WriteByte(S2A_EXTRA_DATA_HAS_GAME_PORT | S2A_EXTRA_DATA_HAS_STEAMID | S2A_EXTRA_DATA_GAMEID | S2A_EXTRA_DATA_HAS_GAMETAG_DATA);

			m_WriteBuf.WriteShort(m_ArgParser.GetOptionValueInt16U("-port"));
			m_WriteBuf.WriteLongLong(SteamGameServer()->GetSteamID().ConvertToUint64());
			m_WriteBuf.WriteString(SERVER_TAG);
			m_WriteBuf.WriteLongLong(SERVER_APPID);

			co_await socket.async_send_to(asio::buffer(m_Buf, m_WriteBuf.GetNumBytesWritten()), remote_endpoint, asio::use_awaitable);
			co_return true;
		}
		case A2S_PLAYER:
		{
			if (CONFIG_HANDLE_QUERY_BY_STEAM)
				co_return false;

			m_WriteBuf.WriteLong(CONNECTIONLESS_HEADER);
			m_WriteBuf.WriteByte(S2A_PLAYER);
			m_WriteBuf.WriteByte(1);
			m_WriteBuf.WriteByte(0);
			m_WriteBuf.WriteString("Max Players");
			m_WriteBuf.WriteLong(SERVER_MAX_CLIENTS);
			m_WriteBuf.WriteFloat(3600.0);
			
			co_await socket.async_send_to(asio::buffer(m_Buf, m_WriteBuf.GetNumBytesWritten()), remote_endpoint, asio::use_awaitable);
			co_return true;
		}
		case A2S_GETCHALLENGE:
		{
			if (!Steam3Server().BHasLogonResult())
				break;

			char temp[512];
			msg.ReadString(temp, sizeof(temp));

			//tiny csgo client wants to authenticate ticket
			if (strcmp(temp, "tiny-csgo-client") == 0)
			{
				auto keyLen = msg.ReadShort();
				msg.ReadBytes(temp, keyLen);

				uint64_t userSteamID = *reinterpret_cast<uint64_t*>((uintptr_t)temp + 12);
				auto result = SteamGameServer()->BeginAuthSession(temp, keyLen, userSteamID);
				printf("BeginAuthSession result for ticket of %llu is %d\n", userSteamID, result);

				m_WriteBuf.WriteLong(CONNECTIONLESS_HEADER);
				m_WriteBuf.WriteByte(S2C_CONNECTION);
			}
			else
			{
				m_WriteBuf.WriteLong(CONNECTIONLESS_HEADER);
				m_WriteBuf.WriteByte(S2C_CHALLENGE);
				m_WriteBuf.WriteLong(SERVER_CHALLENGE);
				m_WriteBuf.WriteLong(PROTOCOL_STEAM);

				m_WriteBuf.WriteShort(0); //  steam2 encryption key not there anymore
				m_WriteBuf.WriteLongLong(SteamGameServer()->GetSteamID().ConvertToUint64());
				m_WriteBuf.WriteByte(Steam3Server().BSecure());

				snprintf(temp, sizeof(temp), "connect0x%X", SERVER_CHALLENGE);
				m_WriteBuf.WriteString(temp);

				m_WriteBuf.WriteLong(13837);
				m_WriteBuf.WriteString(SERVER_PASSWD_NEEDED ? "friends" : "public");
				m_WriteBuf.WriteByte(SERVER_PASSWD_NEEDED);
				m_WriteBuf.WriteLongLong((uint64)-1); //Lobby id
				m_WriteBuf.WriteByte(SERVER_DCFRIENDSREQD);
				m_WriteBuf.WriteByte(SERVER_VALVE_OFFICIAL);
			}
			
			co_await socket.async_send_to(asio::buffer(m_Buf, m_WriteBuf.GetNumBytesWritten()), remote_endpoint, asio::use_awaitable);
			co_return true;
		}
		case C2S_CONNECT:
		{
			//We don't want clients to connect to our server, so reject every connection request
			m_WriteBuf.WriteLong(CONNECTIONLESS_HEADER);
			m_WriteBuf.WriteByte(S2C_CONNREJECT);

			if (m_ArgParser.HasOption("-rdip"))
			{
				m_WriteBuf.WriteString("ConnectRedirectAddress:");
				m_WriteBuf.SeekToBit(m_WriteBuf.GetNumBitsWritten() - 8); //Get rid of '\0'
				m_WriteBuf.WriteString(m_ArgParser.GetOptionValueString("-rdip"));
			}
			else
			{
				m_WriteBuf.WriteString("This server will reject every connection request, don't attempt to connect.");
			}

			co_await socket.async_send_to(asio::buffer(m_Buf, m_WriteBuf.GetNumBytesWritten()), remote_endpoint, asio::use_awaitable);
			co_return true;
		}
		default:
			co_return false;
		}// switch (c)
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
