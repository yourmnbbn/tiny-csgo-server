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
#include "serverinfo.hpp"

using namespace asio::ip;
using namespace std::chrono_literals;

inline asio::io_context g_IoContext;

class Server
{
public:
	Server(ArgParser& parser) :
		m_WriteBuf(m_Buf, sizeof(m_Buf)),
		m_ReadBuf(m_Buf, sizeof(m_Buf)),
		m_ArgParser(parser)

	{
		m_VersionInt = GetIntVersionFromString(parser.GetOptionValueString("-version"));
	}

public:
	void InitializeServer()
	{
		//Connect to steam game server
		Steam3Server().InitServer(m_ArgParser.GetOptionValueInt16U("-port"),
			m_ArgParser.GetOptionValueString("-version"), m_ArgParser.HasOption("-vac"));
		Steam3Server().SetAccount(m_ArgParser.GetOptionValueString("-gslt"));
		Steam3Server().LogOn();

		if (Steam3Server().BLoggedOn())
		{
			//Connect to GC
			g_GCClient.Init();
			g_GCClient.SendHello();
			g_GCClient.SwitchToAsync();

			asio::co_spawn(g_IoContext, PrepareListenServer(), asio::detached);
			asio::co_spawn(g_IoContext, RunFrame(), asio::detached);
			asio::co_spawn(g_IoContext, PrintAuthedCount(), asio::detached);
		}
	}

	void RunServer() { g_IoContext.run(); }

private:
	asio::awaitable<void> PrepareListenServer()
	{
		udp::socket socket(g_IoContext, udp::endpoint(udp::v4(), m_ArgParser.GetOptionValueInt16U("-port")));
		
#ifdef COMPILER_MSVC
		//In some early version of windows, unreachable udp packet will trigger a 10045 error
		DWORD dwBytesReturned = 0;
		BOOL bNewBehavior = FALSE;
		WSAIoctl(socket.native_handle(), SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);
#endif // COMPILER_MSVC

		if (m_ArgParser.HasOption("-mirror") && ResolveRedirectSocket(m_ArgParser.GetOptionValueString("-rdip")))
		{
			m_RedirectEdp = udp::endpoint(make_address_v4(m_RedirectIP), m_RedirectPort);
			asio::co_spawn(g_IoContext, ProcessA2sRequest(socket), asio::detached);
		}

		co_await HandleIncommingPacket(socket);
	}

	asio::awaitable<void>	ProcessA2sRequest(udp::socket& socket)
	{
		while (true)
		{
			//Request challenge here, and send A2S_INFO and A2S_PLAYER request when challenge is received
			m_WriteBuf.Reset();
			m_WriteBuf.WriteLong(CONNECTIONLESS_HEADER);
			m_WriteBuf.WriteByte(A2S_INFO);
			m_WriteBuf.WriteString(A2S_INFO_REQUEST_BODY);
			co_await socket.async_send_to(asio::buffer(m_Buf, m_WriteBuf.GetNumBytesWritten()), m_RedirectEdp, asio::use_awaitable);

			asio::steady_timer timer(g_IoContext, 10s);
			co_await timer.async_wait(asio::use_awaitable);
		}
	}

	asio::awaitable<void> PrintAuthedCount()
	{
		while (true)
		{
			asio::steady_timer timer(g_IoContext, 60s);
			co_await timer.async_wait(asio::use_awaitable);

			printf("Total authenticated players: %d\n", GetAuthHolder().GetAuthedPlayersCount());
		}
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

	asio::awaitable<void> HandleA2sMessage(udp::socket& socket)
	{
		if (m_ReadBuf.ReadLong() != CONNECTIONLESS_HEADER)
			co_return;

		switch (m_ReadBuf.ReadByte())
		{
		case S2C_CHALLENGE:
		{
			auto challenge = m_ReadBuf.ReadLong();
			ResetWriteBuffer();

			//A2S_INFO
			m_WriteBuf.WriteLong(CONNECTIONLESS_HEADER);
			m_WriteBuf.WriteByte(A2S_INFO);
			m_WriteBuf.WriteString(A2S_INFO_REQUEST_BODY);
			m_WriteBuf.WriteLong(challenge);
			co_await socket.async_send_to(asio::buffer(m_Buf, m_WriteBuf.GetNumBytesWritten()), m_RedirectEdp, asio::use_awaitable);

			//A2S_PLAYER
			ResetWriteBuffer();
			m_WriteBuf.WriteLong(CONNECTIONLESS_HEADER);
			m_WriteBuf.WriteByte(A2S_PLAYER);
			m_WriteBuf.WriteLong(challenge);
			co_await socket.async_send_to(asio::buffer(m_Buf, m_WriteBuf.GetNumBytesWritten()), m_RedirectEdp, asio::use_awaitable);
			break;
		}
		case S2A_INFO_SRC:
		{
			char temp[1024];
			auto& info = GetServerInfoHolder();

			info.ServerProtocol() = m_ReadBuf.ReadByte();

			m_ReadBuf.ReadString(temp, sizeof(temp));
			info.ServerName() = temp;
			m_ReadBuf.ReadString(temp, sizeof(temp));
			info.ServerMap() = temp;
			m_ReadBuf.ReadString(temp, sizeof(temp));
			info.ServerGameFolder() = temp;

			//skip the game name and appid, number of players
			m_ReadBuf.ReadString(temp, sizeof(temp));
			m_ReadBuf.ReadShort();
			m_ReadBuf.ReadByte();

			info.ServerMaxClients() = m_ReadBuf.ReadByte();
			info.ServerNumFakeClient() = m_ReadBuf.ReadByte();
			info.ServerType() = m_ReadBuf.ReadByte();
			info.ServerOS() = m_ReadBuf.ReadByte();
			info.ServerPasswordNeeded() = m_ReadBuf.ReadByte();
			info.ServerVacStatus() = m_ReadBuf.ReadByte();

			//version string
			m_ReadBuf.ReadString(temp, sizeof(temp));

			//EDF, we discard all but the game tag
			auto edf = m_ReadBuf.ReadByte();
			if(edf & S2A_EXTRA_DATA_HAS_GAME_PORT)
				m_ReadBuf.ReadShort();

			if (edf & S2A_EXTRA_DATA_HAS_STEAMID)
				m_ReadBuf.ReadLongLong();

			if (edf & S2A_EXTRA_DATA_HAS_SPECTATOR_DATA)
			{
				m_ReadBuf.ReadShort();
				m_ReadBuf.ReadString(temp, sizeof(temp));
			}

			if (edf & S2A_EXTRA_DATA_HAS_GAMETAG_DATA)
			{
				m_ReadBuf.ReadString(temp, sizeof(temp));
				info.ServerTag() = temp;
			}

			break;
		}
		case S2A_PLAYER:
		{
			GetServerInfoHolder().SaveA2sPlayerResponse((char*)(m_Buf + m_ReadBuf.GetNumBytesRead()), m_LastReceivedPacketLength - m_ReadBuf.GetNumBytesRead());
			break;
		}
		default:
			break;
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
			m_LastReceivedPacketLength = co_await socket.async_receive_from(asio::buffer(m_Buf), edp, asio::use_awaitable);

			//This is the packet from our redirect server
			if (edp == m_RedirectEdp)
			{
				co_await HandleA2sMessage(socket);
				continue;
			}

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

		ResetWriteBuffer();
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

			auto& info = GetServerInfoHolder();

			m_WriteBuf.WriteByte(info.ServerProtocol());
			m_WriteBuf.WriteString(info.ServerName().c_str());
			m_WriteBuf.WriteString(info.ServerMap().c_str());
			m_WriteBuf.WriteString(info.ServerGameFolder().c_str());
			m_WriteBuf.WriteString(info.ServerDescription().c_str());

			m_WriteBuf.WriteShort(info.ServerAppID());
			m_WriteBuf.WriteByte(info.ServerNumClients());
			m_WriteBuf.WriteByte(info.ServerMaxClients());
			m_WriteBuf.WriteByte(info.ServerNumFakeClient());
			m_WriteBuf.WriteByte(info.ServerType());
			m_WriteBuf.WriteByte(info.ServerOS());

			m_WriteBuf.WriteByte(info.ServerPasswordNeeded());
			m_WriteBuf.WriteByte(info.ServerVacStatus());
			m_WriteBuf.WriteString(m_ArgParser.GetOptionValueString("-version"));

			//EDF
			m_WriteBuf.WriteByte(S2A_EXTRA_DATA_HAS_GAME_PORT | S2A_EXTRA_DATA_HAS_STEAMID | S2A_EXTRA_DATA_GAMEID | S2A_EXTRA_DATA_HAS_GAMETAG_DATA);

			m_WriteBuf.WriteShort(m_ArgParser.GetOptionValueInt16U("-port"));
			m_WriteBuf.WriteLongLong(SteamGameServer()->GetSteamID().ConvertToUint64());
			m_WriteBuf.WriteString(info.ServerTag().c_str());
			m_WriteBuf.WriteLongLong(info.ServerAppID());

			co_await socket.async_send_to(asio::buffer(m_Buf, m_WriteBuf.GetNumBytesWritten()), remote_endpoint, asio::use_awaitable);
			co_return true;
		}
		case A2S_PLAYER:
		{
			if (CONFIG_HANDLE_QUERY_BY_STEAM)
				co_return false;

			m_WriteBuf.WriteLong(CONNECTIONLESS_HEADER);
			m_WriteBuf.WriteByte(S2A_PLAYER);

			if (GetServerInfoHolder().GetA2sPlayerResponseLength() < 1)
			{
				m_WriteBuf.WriteByte(1);
				m_WriteBuf.WriteByte(0);
				m_WriteBuf.WriteString("Max Players");
				m_WriteBuf.WriteLong(GetServerInfoHolder().ServerMaxClients());
				m_WriteBuf.WriteFloat(3600.0);
			}
			else
			{
				auto& info = GetServerInfoHolder();
				m_WriteBuf.WriteBytes(info.GetA2sPlayerResponse(), info.GetA2sPlayerResponseLength());
			}
			
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
				m_WriteBuf.WriteByte(result == k_EBeginAuthSessionResultOK ? S2C_CONNECTION : S2C_CONNREJECT);
			}
			else
			{
				//We reject the client here so we won't get reject from lobby error.
				if (m_ArgParser.HasOption("-rdip"))
				{
					m_WriteBuf.WriteLong(CONNECTIONLESS_HEADER);
					m_WriteBuf.WriteByte(S2C_CONNREJECT);
					m_WriteBuf.WriteString("ConnectRedirectAddress:");
					m_WriteBuf.SeekToBit(m_WriteBuf.GetNumBitsWritten() - 8); //Get rid of '\0'
					m_WriteBuf.WriteString(m_ArgParser.GetOptionValueString("-rdip"));
				}
				else
				{
					m_WriteBuf.WriteLong(CONNECTIONLESS_HEADER);
					m_WriteBuf.WriteByte(S2C_CHALLENGE);
					m_WriteBuf.WriteLong(SERVER_CHALLENGE);
					m_WriteBuf.WriteLong(PROTOCOL_STEAM);

					m_WriteBuf.WriteShort(0); //  steam2 encryption key not there anymore
					m_WriteBuf.WriteLongLong(SteamGameServer()->GetSteamID().ConvertToUint64());
					m_WriteBuf.WriteByte(SERVER_VAC_STATES);

					snprintf(temp, sizeof(temp), "connect0x%X", SERVER_CHALLENGE);
					m_WriteBuf.WriteString(temp);

					m_WriteBuf.WriteLong(m_VersionInt);
					m_WriteBuf.WriteString(GetServerInfoHolder().ServerPasswordNeeded() ? "friends" : "public");
					m_WriteBuf.WriteByte(GetServerInfoHolder().ServerPasswordNeeded());
					m_WriteBuf.WriteLongLong((uint64)-1); //Lobby id
					m_WriteBuf.WriteByte(SERVER_DCFRIENDSREQD);
					m_WriteBuf.WriteByte(GetServerInfoHolder().ServerIsOfficial());
				}
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
		auto& info = GetServerInfoHolder();
		SteamGameServer()->SetProduct("valve");
		SteamGameServer()->SetModDir(info.ServerGameFolder().c_str());
		SteamGameServer()->SetServerName(info.ServerName().c_str());
		SteamGameServer()->SetGameDescription(info.ServerDescription().c_str());
		SteamGameServer()->SetGameTags(info.ServerTag().c_str());
		SteamGameServer()->SetMapName(info.ServerMap().c_str());
		SteamGameServer()->SetPasswordProtected(info.ServerPasswordNeeded());
		SteamGameServer()->SetMaxPlayerCount(info.ServerMaxClients());
		SteamGameServer()->SetBotPlayerCount(info.ServerNumFakeClient());
		SteamGameServer()->SetSpectatorPort(0);
		SteamGameServer()->SetRegion(SERVER_REGION);
	}

	void UpdateGCInformation()
	{
		CMsgGCCStrike15_v2_MatchmakingServerReservationResponse info;
		info.set_map(GetServerInfoHolder().ServerMap());

		g_GCClient.SendMessageToGC(k_EMsgGCCStrike15_v2_MatchmakingServerReservationResponse, info);
	}

	inline void		ResetWriteBuffer() { m_WriteBuf.Reset(); }
	inline void		ResetReadBuffer() { m_ReadBuf.Seek(0); }

	inline uint32_t GetIntVersionFromString(const char* version)
	{
		char temp[64];
		int source_idx = 0;
		int dest_idx = 0;

		for (; ; ++source_idx)
		{
			if (version[source_idx] == '.')
				continue;

			temp[dest_idx++] = version[source_idx];

			if (version[source_idx + 1] == 0)
			{
				temp[dest_idx] = 0;
				break;
			}
		}

		return atoi(temp);
	}

	inline bool ResolveRedirectSocket(const char* socketString)
	{
		auto len = strlen(socketString) + 1;
		std::unique_ptr<char[]> memBlock = std::make_unique<char[]>(len);
		auto* pData = memBlock.get();
		memcpy(pData, socketString, len);

		uint32_t index = 0;
		for (index = 0; index < len - 1; ++index)
		{
			if (pData[index] == ':')
				break;
		}

		if (index == len)
		{
			printf("Can't resolve CM Server address: %s\n", socketString);
			return false;
		}

		m_RedirectPort = static_cast<uint16_t>(atoi(pData + index + 1));
		pData[index] = 0;
		m_RedirectIP = pData;

		return true;
	}

private:
	char		m_Buf[10240];
	bf_write	m_WriteBuf;
	bf_read		m_ReadBuf;

	ArgParser&	m_ArgParser;

	uint32_t	m_LastReceivedPacketLength;
	uint32_t	m_VersionInt = 0;

	std::string m_RedirectIP;
	uint16_t	m_RedirectPort;

	udp::endpoint m_RedirectEdp;
};

#endif // !__TINY_CSGO_SERVER_HPP__
