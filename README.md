# tiny-csgo-server
Tiny csgo server for logging on to steam game servers and establish connection with GC, not for real server purpose. This is a highly experimental project. Stable is not guaranteed. 

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
- **Not avalible for now. You can play with this on your own.**

## Command option notes
- `-port` Game server listening port.
- `-version` Version of current csgo, you can find this value in `steam.inf` with key name **"PatchVersion"**.
- `-gslt` Game server logon token.
