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
			printf("Assigned anonymous gameserver Steam ID %llu.\n", m_SteamIDGS.ConvertToUint64());
		}
		else if (m_SteamIDGS.BPersistentGameServerAccount())
		{
			printf("Assigned persistent gameserver Steam ID %llu.\n", m_SteamIDGS.ConvertToUint64());
		}
		else
		{
			printf("Assigned Steam ID %llu, which is of an unexpected type!\n", m_SteamIDGS.ConvertToUint64());
			printf("Unexpected steam ID type!\n");
		}
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

	SteamGameServer()->SetProduct("csgo");
	SteamGameServer()->SetDedicatedServer(true);

	m_bInitialized = true;
}

inline void CSteam3Server::LogOn()
{
	uint32_t retry = 0;
	printf("Logon using gslt %s\n", m_sAccountToken.c_str());
	printf("Waiting for logon result response, this may take a while...\n");
	while (!m_bLogOnResult)
	{
		if (retry++ > 60)
		{
			printf("Can't logon to steam game server after 60 retries!\n");
			break;
		}

		SteamGameServer()->LogOn(m_sAccountToken.c_str());
		std::this_thread::sleep_for(1s);
		SteamGameServer_RunCallbacks();
	}
}

#endif // !__TINY_CSGO_SERVER_STEAMAUTH_HPP__
