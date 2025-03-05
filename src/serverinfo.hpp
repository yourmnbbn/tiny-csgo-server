#ifndef SERVERINFO_HPP
#define SERVERINFO_HPP

#include <string>

class ServerInfo {
public:
    // Конструктор
    ServerInfo(int port, const std::string& gameDir, const std::string& currentMap, int maxPlayers)
        : port(port), gameDir(gameDir), currentMap(currentMap), maxPlayers(maxPlayers), numPlayers(0) {}

    // Геттеры для получения информации о сервере
    int GetPort() const { return port; }
    const std::string& GetGameDir() const { return gameDir; }
    const std::string& GetCurrentMap() const { return currentMap; }
    int GetMaxPlayers() const { return maxPlayers; }
    int GetNumPlayers() const { return numPlayers; }

    // Обновление информации о сервере
    void UpdateServerInfo(const std::string& newMap, int newNumPlayers) {
        currentMap = newMap;
        numPlayers = newNumPlayers;
    }

private:
    int port;
    std::string gameDir;
    std::string currentMap;
    int maxPlayers;
    int numPlayers;
};

#endif // SERVERINFO_HPP
