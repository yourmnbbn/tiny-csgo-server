# tiny-csgo-server
Tiny csgo server for logging on to steam game servers and establish connection with GC, being displayed on the game server browser, not for real server purpose. This is a highly experimental project. Stable is not guaranteed. 

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

## FAQ
### 1. Client can't redirect to the target server, always get `#Valve_Reject_Connect_From_Lobby` error.
For solution to redirecting not working please refer to [this issue](https://github.com/yourmnbbn/tiny-csgo-server/issues/5).

### 2. Player count of my server in the browser is always 0
You can manually control the player count outside of the Internet tab of the browser, where player has to be authenticated by the server then the count number will change. [This](https://github.com/yourmnbbn/tiny-csgo-server) can help to authenticate players.
