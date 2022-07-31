#ifndef __TINY_CSGO_SERVER_INFO_CONST_HPP__
#define __TINY_CSGO_SERVER_INFO_CONST_HPP__

#ifdef _WIN32
#pragma once
#endif

#define _DECL_CONST inline constexpr auto

//Configs
_DECL_CONST CONFIG_HANDLE_QUERY_BY_STEAM = 1;

//Server information
_DECL_CONST SERVER_NAME = "Tiny csgo server";
_DECL_CONST SERVER_MAP = "de_tinycsgomap";
_DECL_CONST SERVER_GAME_FOLDER = "csgo";
_DECL_CONST SERVER_DESCRIPTION = "Counter-Strike: Global Offensive";
_DECL_CONST SERVER_APPID = 730;
_DECL_CONST SERVER_NUM_CLIENTS = 10;
_DECL_CONST SERVER_MAX_CLIENTS = 12;
_DECL_CONST SERVER_NUM_FAKE_CLIENTS = 0;
_DECL_CONST SERVER_TYPE = 'd';
_DECL_CONST SERVER_OS = 'w';
_DECL_CONST SERVER_PASSWD_NEEDED = 0;
_DECL_CONST SERVER_VAC_STATES = 1;
_DECL_CONST SERVER_TAG = "pure, vac, tiny-csgo-server";
_DECL_CONST SERVER_DCFRIENDSREQD = 0;
_DECL_CONST SERVER_VALVE_OFFICIAL = 0;

_DECL_CONST SERVER_CHALLENGE = 0xdeadbeef;

//Other defines 
#define PROTOCOL_AUTHCERTIFICATE 0x01   // Connection from client is using a WON authenticated certificate
#define PROTOCOL_HASHEDCDKEY     0x02	// Connection from client is using hashed CD key because WON comm. channel was unreachable
#define PROTOCOL_STEAM			 0x03	// Steam certificates
#define PROTOCOL_LASTVALID       0x03    // Last valid protocol

#endif // !__TINY_CSGO_SERVER_INFO_CONST_HPP__
