
#include "src/server.hpp"
#include <iostream>
#include <string>
#include <cstdlib>

int main(int argc, char* argv[]) {
    try {
        if (argc < 6 || std::string(argv[1]) != "-port" || std::string(argv[3]) != "-rdip" || std::string(argv[5]) != "-mirror") {
            std::cerr << "Usage: ./tiny-csgo-server -port <port> -rdip <redirect_ip:port> -mirror\n";
            return 1;
        }

        uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));
        std::string redirect_ip_port = argv[4];
        size_t colon_pos = redirect_ip_port.find(':');
        if (colon_pos == std::string::npos) {
            std::cerr << "Invalid redirect IP:PORT format\n";
            return 1;
        }

        std::string redirect_ip = redirect_ip_port.substr(0, colon_pos);
        uint16_t redirect_port = static_cast<uint16_t>(std::stoi(redirect_ip_port.substr(colon_pos + 1)));

        asio::io_context io_context;
        GoldSourceServer server(io_context, port, redirect_ip, redirect_port);
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
