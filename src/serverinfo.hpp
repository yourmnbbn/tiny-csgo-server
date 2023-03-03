#ifndef __TINY_CSGO_SERVER_SERVERINFO_HPP__
#define __TINY_CSGO_SERVER_SERVERINFO_HPP__

#include <string>
#include "common/info_const.hpp"

class ServerInfoHolder
{
public:
	std::string& ServerName() { return m_ServerName; }
	std::string& ServerMap() { return m_ServerMap; }
	std::string& ServerGameFolder() { return m_ServerGameFolder; }
	std::string& ServerDescription() { return m_ServerDescription; }
	constexpr uint16_t ServerAppID() { return SERVER_APPID; }
	const uint8_t ServerNumClients() { return m_ServerMaxClients - 1; }
	uint8_t& ServerMaxClients() { return m_ServerMaxClients; }
	uint8_t& ServerNumFakeClient() { return m_ServerNumFakeClients; }
	uint8_t& ServerType() { return m_ServerType; }
	uint8_t& ServerOS() { return m_ServerOS; }
	uint8_t& ServerProtocol() { return m_ServerProtocol; }
	bool& ServerPasswordNeeded() { return m_ServerPasswdNeeded; }
	bool& ServerVacStatus() { return m_ServerVacStatus; }
	bool& ServerIsOfficial() { return m_ServerIsOfficial; }
	std::string& ServerTag() { return m_ServerTag; }

	void SaveA2sPlayerResponse(const char* pData, size_t length)
	{
		if (length > sizeof(m_A2sPlayerResponse))
		{
			printf("The size of the A2S_PLAYER response %d is too large\n", length);
			return;
		}

		memcpy(m_A2sPlayerResponse, pData, length);
		m_A2sPlayerResponseLength = length;
	}

	char* GetA2sPlayerResponse()
	{
		return m_A2sPlayerResponse;
	}

	size_t GetA2sPlayerResponseLength()
	{
		return m_A2sPlayerResponseLength;
	}

private:
	std::string		m_ServerName			= SERVER_NAME;
	std::string		m_ServerMap				= SERVER_MAP;
	std::string		m_ServerGameFolder		= SERVER_GAME_FOLDER;
	std::string		m_ServerDescription		= SERVER_DESCRIPTION;
	uint8_t			m_ServerNumClients		= SERVER_NUM_CLIENTS;
	uint8_t			m_ServerMaxClients		= SERVER_MAX_CLIENTS;
	uint8_t			m_ServerNumFakeClients	= SERVER_NUM_FAKE_CLIENTS;
	uint8_t			m_ServerType			= SERVER_TYPE;
	uint8_t			m_ServerOS				= SERVER_OS;
	uint8_t			m_ServerProtocol		= SERVER_PROTOCOL;
	bool			m_ServerPasswdNeeded	= SERVER_PASSWD_NEEDED;
	bool			m_ServerVacStatus		= SERVER_VAC_STATES;
	bool			m_ServerIsOfficial		= SERVER_VALVE_OFFICIAL;
	std::string		m_ServerTag				= SERVER_TAG;

	char	m_A2sPlayerResponse[20480];
	size_t	m_A2sPlayerResponseLength = 0;
};

static inline ServerInfoHolder s_ServerInfoHolder;

inline ServerInfoHolder& GetServerInfoHolder()
{
	return s_ServerInfoHolder;
}

#endif // !__TINY_CSGO_SERVER_SERVERINFO_HPP__

