#include "server.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <string>
#include <boost/asio.hpp>

// Регистрация на всех мастер-серверах
void GoldSourceServer::register_with_masters() {
    for (const auto& master : masterServers) {
        char infoString[512];
        snprintf(infoString, sizeof(infoString),
                 "\\infoRequest\\port\\%d\\gamedir\\%s\\map\\%s\\maxplayers\\%d\\players\\%d\\protocol\\%d\\",
                 serverInfo.GetPort(), serverInfo.GetGameDir().c_str(), serverInfo.GetCurrentMap().c_str(),
                 serverInfo.GetMaxPlayers(), serverInfo.GetNumPlayers(), PROTOCOL_VERSION);

        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return;
        }

        struct sockaddr_in masterAddr;
        memset(&masterAddr, 0, sizeof(masterAddr));
        masterAddr.sin_family = AF_INET;
        masterAddr.sin_port = htons(27010); // Порт мастер-сервера Steam
        inet_pton(AF_INET, master.c_str(), &masterAddr.sin_addr);

        sendto(sockfd, infoString, strlen(infoString), 0, (struct sockaddr*)&masterAddr, sizeof(masterAddr));
        close(sockfd);
    }
}

// Асинхронный прием пакетов
void GoldSourceServer::listen_for_packets() {
    socket_.async_receive_from(
        asio::buffer(recv_buffer_), sender_endpoint_,
        [this](std::error_code ec, std::size_t bytes_recvd) {
            if (!ec && bytes_recvd > 0) {
                process_packet(bytes_recvd);
            }
            listen_for_packets();
        });
}

// Обработка входящих пакетов
void GoldSourceServer::process_packet(std::size_t length) {
    if (length < 5 || recv_buffer_[0] != '\xFF' || recv_buffer_[1] != '\xFF' || recv_buffer_[2] != '\xFF' || recv_buffer_[3] != '\xFF') {
        return; // Не валидный пакет GoldSource
    }

    std::string request(recv_buffer_.data() + 4, length - 4);
    if (request.starts_with("TSource Engine Query")) {
        send_a2s_info();
    } else if (request.starts_with("\x55")) {
        send_a2s_challenge();
    } else if (request.starts_with("\x57")) {
        send_redirect();
    }
}

// Отправка A2S_INFO
void GoldSourceServer::send_a2s_info() {
    std::vector<char> response = {
        '\xFF', '\xFF', '\xFF', '\xFF', '\x49',
        48, // Версия протокола (исправлено на 48)
    };

    // Подробности сервера
    const std::string server_name = "Redirect Server";
    const std::string map_name = "de_dust2";
    const std::string game_dir = "cstrike";
    const std::string game_desc = "Counter-Strike 1.6";
    uint16_t player_count = 0;
    uint16_t max_players = 32;
    uint16_t bot_count = 0;
    char server_type = 'd'; // Dedicated
    char environment = 'l'; // Linux
    char visibility = 0;    // Public
    char vac = 1;           // VAC secured

    // Добавление деталей в ответ
    response.insert(response.end(), server_name.begin(), server_name.end());
    response.push_back('\0');
    response.insert(response.end(), map_name.begin(), map_name.end());
    response.push_back('\0');
    response.insert(response.end(), game_dir.begin(), game_dir.end());
    response.push_back('\0');
    response.insert(response.end(), game_desc.begin(), game_desc.end());
    response.push_back('\0');

    // Информация о игроках и сервере
    response.push_back(static_cast<char>(player_count));
    response.push_back(static_cast<char>(max_players));
    response.push_back(static_cast<char>(bot_count));
    response.push_back(server_type);
    response.push_back(environment);
    response.push_back(visibility);
    response.push_back(vac);

    // Дополнительные данные
    const std::string version = "48"; // Версия протокола
    response.insert(response.end(), version.begin(), version.end());
    response.push_back('\0');

    socket_.send_to(asio::buffer(response), sender_endpoint_);
}

// Отправка A2S_CHALLENGE
void GoldSourceServer::send_a2s_challenge() {
    std::vector<char> response = {'\xFF', '\xFF', '\xFF', '\xFF', '\x41'};
    int32_t challenge = rand();
    response.push_back((challenge >> 24) & 0xFF);
    response.push_back((challenge >> 16) & 0xFF);
    response.push_back((challenge >> 8) & 0xFF);
    response.push_back(challenge & 0xFF);
    socket_.send_to(asio::buffer(response), sender_endpoint_);
}

// Отправка редиректа
void GoldSourceServer::send_redirect() {
    std::string redirect_message = "\xFF\xFF\xFF\xFF\x30"; // Заголовок редиректа
    redirect_message += redirect_ip_ + ":" + std::to_string(redirect_port_);
    socket_.send_to(asio::buffer(redirect_message), sender_endpoint_);
}
