#ifndef __TINY_CSGO_SERVER_STEAMAUTH_HPP__
#define __TINY_CSGO_SERVER_STEAMAUTH_HPP__

#ifdef _WIN32
#pragma once
#endif

#include <string>
#include <thread>

#include <chrono>
#include <steam_api.h>
#include <steam_gameserver.h>

using namespace std::chrono_literals;

class CSteam3Server : public CSteamGameServerAPIContext
{
public:
	CSteam3Server()
		:m_CallbackLogonSuccess(this, &CSteam3Server::OnLogonSuccess),
		m_CallbackLogonFailure(this, &CSteam3Server::OnLogonFailure),
		m_CallbackLoggedOff(this, &CSteam3Server::OnLoggedOff),
		m_CallbackValidateAuthTicketResponse(this, &CSteam3Server::OnValidateAuthTicketResponse),
		m_CallbackGSPolicyResponse(this, &CSteam3Server::OnGSPolicyResponse)
	{
	}

	~CSteam3Server() {};

	STEAM_GAMESERVER_CALLBACK(CSteam3Server, OnLogonSuccess, SteamServersConnected_t, m_CallbackLogonSuccess);
	STEAM_GAMESERVER_CALLBACK(CSteam3Server, OnLogonFailure, SteamServerConnectFailure_t, m_CallbackLogonFailure);
	STEAM_GAMESERVER_CALLBACK(CSteam3Server, OnLoggedOff, SteamServersDisconnected_t, m_CallbackLoggedOff);
	// game server callbacks
	STEAM_GAMESERVER_CALLBACK(CSteam3Server, OnGSPolicyResponse, GSPolicyResponse_t, m_CallbackGSPolicyResponse);
	STEAM_GAMESERVER_CALLBACK(CSteam3Server, OnValidateAuthTicketResponse, ValidateAuthTicketResponse_t, m_CallbackValidateAuthTicketResponse);

	// CSteam3 stuff
	bool				BSecure() { return SteamGameServer() && SteamGameServer()->BSecure(); }
	bool				BIsActive() { return SteamGameServer() && (m_eServerMode >= eServerModeNoAuthentication); }
	bool				BLanOnly() const { return m_eServerMode == eServerModeNoAuthentication; }
	bool				BWantsSecure() { return m_eServerMode == eServerModeAuthenticationAndSecure; }
	bool				BLoggedOn() { return SteamGameServer() && SteamGameServer()->BLoggedOn(); }
	const CSteamID&		GetGSSteamID() const { return m_SteamIDGS; };
	bool				BHasLogonResult() const { return m_bLogOnResult; }
	void				LogOnAnonymous() { if (SteamGameServer())	SteamGameServer()->LogOnAnonymous(); }
	uint16				GetQueryPort() const { return m_QueryPort; }

	// Fetch public IP.  Might return 0 if we don't know
	uint32				GetPublicIP() { return SteamGameServer() ? SteamGameServer()->GetPublicIP().m_unIPv4 : 0; }

	/// Select Steam account name / password to use
	void				SetAccount(const char* pszToken) { m_sAccountToken = pszToken; }

	/// What account name was selected?
	const char*			GetAccountToken() const { return m_sAccountToken.c_str(); }

	void				InitServer(uint16_t port, const char* version);
	void				LogOn();

private:
	EServerMode	m_eServerMode;

	bool m_bMasterServerUpdaterSharingGameSocket;
	bool m_bLogOnFinished;
	bool m_bLoggedOn;
	bool		m_bLogOnResult;		// if true, show logon result
	bool		m_bHasActivePlayers;  // player stats updates are only sent if active players are available
	CSteamID	m_SteamIDGS;
	bool m_bInitialized;

	// The port that we are listening for queries on.
	uint32		m_unIP;
	uint16		m_usPort;
	uint16		m_QueryPort;
	std::string m_sAccountToken;
};

inline CSteam3Server s_SteamServer;
inline CSteam3Server& Steam3Server()
{
	return s_SteamServer;
}

inline void CSteam3Server::OnValidateAuthTicketResponse(ValidateAuthTicketResponse_t* pValidateAuthTicketResponse)
{
	printf("OnValidateAuthTicketResponse\n");
}

inline void CSteam3Server::OnGSPolicyResponse(GSPolicyResponse_t* pPolicyResponse)
{
	if (!BIsActive())
		return;

	if (SteamGameServer() && SteamGameServer()->BSecure())
	{
		printf("VAC secure mode is activated.\n");
	}
	else
	{
		printf("VAC secure mode disabled.\n");
	}
}


inline void CSteam3Server::OnLogonSuccess(SteamServersConnected_t* pLogonSuccess)
{
	if (!BIsActive())
		return;

	if (!m_bLogOnResult)
	{
		m_bLogOnResult = true;
	}

	printf("Connection to Steam servers successful.\n");
	if (SteamGameServer())
	{
		uint32 ip = SteamGameServer()->GetPublicIP().m_unIPv4;
		printf("   Public IP is %d.%d.%d.%d.\n", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);

		m_SteamIDGS = SteamGameServer()->GetSteamID();

		if (m_SteamIDGS.BAnonGameServerAccount())
		{
			printf("Assigned anonymous gameserver Steam ID %s.\n", m_SteamIDGS.Render());
		}
		else if (m_SteamIDGS.BPersistentGameServerAccount())
		{
			printf("Assigned persistent gameserver Steam ID %s.\n", m_SteamIDGS.Render());
		}
		else
		{
			printf("Assigned Steam ID %s, which is of an unexpected type!\n", m_SteamIDGS.Render());
			printf("Unexpected steam ID type!\n");
		}

		printf("Gameserver logged on to Steam, assigned identity steamid:%llu\n", m_SteamIDGS.ConvertToUint64());
	}
}

inline void CSteam3Server::OnLogonFailure(SteamServerConnectFailure_t* pLogonFailure)
{
	if (!BIsActive())
		return;

	if (!m_bLogOnResult)
	{
		char const* szFatalError = NULL;
		switch (pLogonFailure->m_eResult)
		{
		case k_EResultAccountNotFound:	// account not found
			szFatalError = "*  Invalid Steam account token was specified.      *\n";
			break;
		case k_EResultGSLTDenied:		// a game server login token owned by this token's owner has been banned
			szFatalError = "*  Steam account token specified was revoked.      *\n";
			break;
		case k_EResultGSOwnerDenied:	// game server owner is denied for other reason (account lock, community ban, vac ban, missing phone)
			szFatalError = "*  Steam account token owner no longer eligible.   *\n";
			break;
		case k_EResultGSLTExpired:		// this game server login token was disused for a long time and was marked expired
			szFatalError = "*  Steam account token has expired from inactivity.*\n";
			break;
		case k_EResultServiceUnavailable:
			if (!BLanOnly())
			{
				printf("Connection to Steam servers successful (SU).\n");
			}
			break;
		default:
			if (!BLanOnly())
			{
				printf("Could not establish connection to Steam servers.\n");
			}
			break;
		}

		printf("Could not establish connection to Steam servers, error code : %d\n", pLogonFailure->m_eResult);
		if (szFatalError)
		{
			printf("****************************************************\n");
			printf("*                FATAL ERROR                       *\n");
			printf("%s", szFatalError);
			printf("*  Double-check your sv_setsteamaccount setting.   *\n");
			printf("*                                                  *\n");
			printf("*  To create a game server account go to           *\n");
			printf("*  http://steamcommunity.com/dev/managegameservers *\n");
			printf("*                                                  *\n");
			printf("****************************************************\n");
		}
	}

	m_bLogOnResult = true;
}

inline void CSteam3Server::OnLoggedOff(SteamServersDisconnected_t* pLoggedOff)
{
	if (!BLanOnly())
	{
		printf("Connection to Steam servers lost.  (Result = %d)\n", pLoggedOff->m_eResult);
	}

	if (GetGSSteamID().BPersistentGameServerAccount())
	{
		switch (pLoggedOff->m_eResult)
		{
		case k_EResultLoggedInElsewhere:
		case k_EResultLogonSessionReplaced:
			printf("****************************************************\n");
			printf("*                                                  *\n");
			printf("*  Steam account token was reused elsewhere.       *\n");
			printf("*  Make sure you are using a separate account      *\n");
			printf("*  token for each game server that you operate.    *\n");
			printf("*                                                  *\n");
			printf("*  To create additional game server accounts go to *\n");
			printf("*  http://steamcommunity.com/dev/managegameservers *\n");
			printf("*                                                  *\n");
			printf("*  This game server instance will now shut down!   *\n");
			printf("*                                                  *\n");
			printf("****************************************************\n");
			return;
		}
	}
}

inline void CSteam3Server::InitServer(uint16_t port, const char* version)
{
	m_eServerMode = eServerModeAuthenticationAndSecure;

	if (!SteamGameServer_Init(0, port, STEAMGAMESERVER_QUERY_PORT_SHARED, m_eServerMode, version))
		printf("[SteamGameServer] Unable to initialize Steam Game Server.\n");
	else
		printf("[SteamGameServer] Initialize Steam Game Server success.\n");

	if (!Init())
		printf("[CSteamGameServerAPIContext] initialize failed!\n");

	if (!SteamGameServer())
	{
		printf("SteamGameServer() == nullptr\n");
	}

	SteamGameServer()->SetProduct("valve");
	SteamGameServer()->SetDedicatedServer(true);

	m_bInitialized = true;
}

inline void CSteam3Server::LogOn()
{
	switch (m_eServerMode)
	{
	case eServerModeNoAuthentication:
		printf("Initializing Steam libraries for LAN server\n");
		break;
	case eServerModeAuthentication:
		printf("Initializing Steam libraries for INSECURE Internet server.  Authentication and VAC not requested.\n");
		break;
	case eServerModeAuthenticationAndSecure:
		printf("Initializing Steam libraries for secure Internet server\n");
		break;
	default:
		printf("Bogus eServermode %d!\n", m_eServerMode);
		printf("Bogus server mode?!");
		break;
	}

	if (m_sAccountToken.empty())
	{
		printf("****************************************************\n");
		printf("*                                                  *\n");
		printf("*  No Steam account token was specified.           *\n");
		printf("*  Logging into anonymous game server account.     *\n");
		printf("*  Connections will be restricted to LAN only.     *\n");
		printf("*                                                  *\n");
		printf("*  To create a game server account go to           *\n");
		printf("*  http://steamcommunity.com/dev/managegameservers *\n");
		printf("*                                                  *\n");
		printf("****************************************************\n");

		SteamGameServer()->LogOnAnonymous();
	}
	else
	{
		printf("Logging into Steam gameserver account with logon token %s\n", m_sAccountToken.c_str());
		SteamGameServer()->LogOn(m_sAccountToken.c_str());
	}

	printf("Waiting for logon result response, this may take a while...\n");

	uint32_t retry = 0;
	while (!m_bLogOnResult)
	{
		if (retry++ > 60)
		{
			printf("Can't logon to steam game server after 60 retries!\n");
			break;
		}

		std::this_thread::sleep_for(1s);
		SteamGameServer_RunCallbacks();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Renders the steam ID to a buffer.  NOTE: for convenience of calling
//			code, this code returns a pointer to a static buffer and is NOT thread-safe.
// Output:  buffer with rendered Steam ID
//-----------------------------------------------------------------------------
inline const char* CSteamID::Render() const
{
	// longest length of returned string is k_cBufLen
	//	[A:%u:%u:%u]
	//	 %u == 10 * 3 + 6 == 36, plus terminator == 37
	const int k_cBufLen = 37;

	const int k_cBufs = 4;	// # of static bufs to use (so people can compose output with multiple calls to Render() )
	static char rgchBuf[k_cBufs][k_cBufLen];
	static int nBuf = 0;
	char* pchBuf = rgchBuf[nBuf];	// get pointer to current static buf
	nBuf++;	// use next buffer for next call to this method
	nBuf %= k_cBufs;

	if (k_EAccountTypeAnonGameServer == m_steamid.m_comp.m_EAccountType)
	{
		snprintf(pchBuf, k_cBufLen, "[A:%u:%u:%u]", m_steamid.m_comp.m_EUniverse, m_steamid.m_comp.m_unAccountID, m_steamid.m_comp.m_unAccountInstance);
	}
	else if (k_EAccountTypeGameServer == m_steamid.m_comp.m_EAccountType)
	{
		snprintf(pchBuf, k_cBufLen, "[G:%u:%u]", m_steamid.m_comp.m_EUniverse, m_steamid.m_comp.m_unAccountID);
	}
	else if (k_EAccountTypeMultiseat == m_steamid.m_comp.m_EAccountType)
	{
		snprintf(pchBuf, k_cBufLen, "[M:%u:%u:%u]", m_steamid.m_comp.m_EUniverse, m_steamid.m_comp.m_unAccountID, m_steamid.m_comp.m_unAccountInstance);
	}
	else if (k_EAccountTypePending == m_steamid.m_comp.m_EAccountType)
	{
		snprintf(pchBuf, k_cBufLen, "[P:%u:%u]", m_steamid.m_comp.m_EUniverse, m_steamid.m_comp.m_unAccountID);
	}
	else if (k_EAccountTypeContentServer == m_steamid.m_comp.m_EAccountType)
	{
		snprintf(pchBuf, k_cBufLen, "[C:%u:%u]", m_steamid.m_comp.m_EUniverse, m_steamid.m_comp.m_unAccountID);
	}
	else if (k_EAccountTypeClan == m_steamid.m_comp.m_EAccountType)
	{
		// 'g' for "group"
		snprintf(pchBuf, k_cBufLen, "[g:%u:%u]", m_steamid.m_comp.m_EUniverse, m_steamid.m_comp.m_unAccountID);
	}
	else if (k_EAccountTypeChat == m_steamid.m_comp.m_EAccountType)
	{
		if (m_steamid.m_comp.m_unAccountInstance & k_EChatInstanceFlagClan)
		{
			snprintf(pchBuf, k_cBufLen, "[c:%u:%u]", m_steamid.m_comp.m_EUniverse, m_steamid.m_comp.m_unAccountID);
		}
		else if (m_steamid.m_comp.m_unAccountInstance & k_EChatInstanceFlagLobby)
		{
			snprintf(pchBuf, k_cBufLen, "[L:%u:%u]", m_steamid.m_comp.m_EUniverse, m_steamid.m_comp.m_unAccountID);
		}
		else // Anon chat
		{
			snprintf(pchBuf, k_cBufLen, "[T:%u:%u]", m_steamid.m_comp.m_EUniverse, m_steamid.m_comp.m_unAccountID);
		}
	}
	else if (k_EAccountTypeInvalid == m_steamid.m_comp.m_EAccountType)
	{
		snprintf(pchBuf, k_cBufLen, "[I:%u:%u]", m_steamid.m_comp.m_EUniverse, m_steamid.m_comp.m_unAccountID);
	}
	else if (k_EAccountTypeIndividual == m_steamid.m_comp.m_EAccountType)
	{
		const unsigned int k_unSteamUserDesktopInstance = 1;
		if (m_steamid.m_comp.m_unAccountInstance != k_unSteamUserDesktopInstance)
			snprintf(pchBuf, k_cBufLen, "[U:%u:%u:%u]", m_steamid.m_comp.m_EUniverse, m_steamid.m_comp.m_unAccountID, m_steamid.m_comp.m_unAccountInstance);
		else
			snprintf(pchBuf, k_cBufLen, "[U:%u:%u]", m_steamid.m_comp.m_EUniverse, m_steamid.m_comp.m_unAccountID);
	}
	else if (k_EAccountTypeAnonUser == m_steamid.m_comp.m_EAccountType)
	{
		snprintf(pchBuf, k_cBufLen, "[a:%u:%u]", m_steamid.m_comp.m_EUniverse, m_steamid.m_comp.m_unAccountID);
	}
	else
	{
		snprintf(pchBuf, k_cBufLen, "[i:%u:%u]", m_steamid.m_comp.m_EUniverse, m_steamid.m_comp.m_unAccountID);
	}
	return pchBuf;
}


#endif // !__TINY_CSGO_SERVER_STEAMAUTH_HPP__
