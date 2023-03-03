# tiny-csgo-server
Tiny csgo server for logging on to steam game servers and establish connection with GC, being displayed on the game server browser, not for real server purpose. It's a fake server.

## Dependencies
 - [hl2sdk-csgo](https://github.com/alliedmodders/hl2sdk/tree/csgo)
 - [Asio](https://github.com/chriskohlhoff/asio) 
 - CMake

## Compile and Run 
### Windows
1. Configure path of hl2sdk-csgo and Asio in `build.bat`.
2. Run `build.bat` to compile the project.
3. Move every file in bin/windows next to `tiny-csgo-server.exe`.
4. Run `tiny-csgo-server.exe` with necessary commandline.

### Linux
1. Configure path of hl2sdk-csgo and Asio in `build.sh`.
2. Run `build.sh` to compile the project.
3. Move every file in bin/linux next to `tiny-csgo-server`.
4. Set the directory constains your executable to LD_LIBRARY_PATH.
5. Run `tiny-csgo-server` with necessary commandline.

## Command option notes
- `-port` Game server listening port.
- `-version` Version of current csgo, you can find this value in `steam.inf` with key name **"PatchVersion"**.
- `-gslt` Game server logon token. If you don't set this, game server will logon to anonymous account and will not be displayed in the internet server browser.
- `-rdip` Redirect IP Address (e.g. 127.0.0.1:27015). If this is set, server will redirect all connection request to the target address. If this is not set, server will reject all connection request.
- `-vac` With this option to enable vac, without to disable.
- `-mirror` With this option to enable the displaying of the target redirect server's information and players. The server name, map, max players, player list etc, are going to be the same with the redirect server. The duplicated information is updated every 10 seconds.

## Special notice if you're trying to use tiny-steam-client
You have to disable vac, which means without option `-vac` to fake online players. But you can change the information variable `SERVER_VAC_STATES = 1` to fake a vac enabled status in the browser. 

## Example usage
Assuming the GSLT is `5AB2234D9C490DCG406AET0763DE5813`, game server port is `27015`, server version is `1.38.5.5`.  

```
Windows:
tiny-csgo-server.exe -port 27015 -version 1.38.5.5 -gslt 5AB2234D9C490DCG406AET0763DE5813

Linux:
./tiny-csgo-server -port 27015 -version 1.38.5.5 -gslt 5AB2234D9C490DCG406AET0763DE5813
```

- If you want to redirect the players trying to connect this tiny server, assuming the target server is `127.0.0.1:27016`

```
Windows:
tiny-csgo-server.exe -port 27015 -version 1.38.5.5 -gslt 5AB2234D9C490DCG406AET0763DE5813 -rdip 127.0.0.1:27016

Linux:
./tiny-csgo-server -port 27015 -version 1.38.5.5 -gslt 5AB2234D9C490DCG406AET0763DE5813 -rdip 127.0.0.1:27016
```

- If you want to redirect the players trying to connect this tiny server, and copy the information of the target server and online players, assuming the target server is `127.0.0.1:27016`

```
Windows:
tiny-csgo-server.exe -port 27015 -version 1.38.5.5 -gslt 5AB2234D9C490DCG406AET0763DE5813 -rdip 127.0.0.1:27016 -mirror

Linux:
./tiny-csgo-server -port 27015 -version 1.38.5.5 -gslt 5AB2234D9C490DCG406AET0763DE5813 -rdip 127.0.0.1:27016 -mirror
```

## How to change server information
In `src/common/info_const.hpp`, you can change the following constances to change the information. In the future, all these information will be configurable through a cfg file. Note that incorrect value of some variables may keep your fake server from being displayed in the browser. **Or you can just use -mirror option to copy the redirect target server's information, when -mirror is enabled, information below is overridden.**
```cpp
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
_DECL_CONST SERVER_REGION = SERVER_REGION_ASIA;
```

## FAQ
### 1. Client can't redirect to the target server, always get `#Valve_Reject_Connect_From_Lobby` error.
For solution to redirecting not working please refer to [this issue](https://github.com/yourmnbbn/tiny-csgo-server/issues/5).

### 2. Player count of my server in the browser is always 0
You can manually control the player count outside of the Internet tab of the browser, where player has to be authenticated by the server then the count number will change. [This](https://github.com/yourmnbbn/tiny-steam-client) can help to authenticate players.
