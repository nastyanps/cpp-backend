#ifdef _WIN32
#define _WINSOCKAPI_
#include <winsock2.h>
#endif

#include "audio.h"
#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace net = boost::asio;
using net::ip::udp;

using namespace std::literals;

void StartServer(uint16_t port) {
    net::io_context io_context;

    // Создаём UDP сокет и привязываем к порту
    udp::socket socket(io_context, udp::endpoint(udp::v4(), port));
    std::cout << "Server started on port " << port << std::endl;

    Player player(ma_format_u8, 1);
    const int frame_size = player.GetFrameSize();

    while (true) {
        // Буфер для приёма данных (максимум 65000 фреймов)
        std::vector<char> buffer(65000 * frame_size);

        boost::system::error_code ec;
        udp::endpoint sender_endpoint;

        // Принимаем датаграмму
        size_t received_bytes = socket.receive_from(
            net::buffer(buffer), sender_endpoint, 0, ec);

        if (ec) {
            std::cout << "Error receiving data: " << ec.message() << std::endl;
            continue;
        }

        // Вычисляем количество фреймов
        size_t frames = received_bytes / frame_size;

        std::cout << "Received " << received_bytes << " bytes ("
            << frames << " frames) from "
            << sender_endpoint.address().to_string() << std::endl;

        // Воспроизводим звук
        player.PlayBuffer(buffer.data(), frames, 1.5s);
        std::cout << "Playing done" << std::endl;
    }
}

void StartClient(uint16_t port) {
    net::io_context io_context;

    // Создаём UDP сокет (без привязки к конкретному порту)
    udp::socket socket(io_context, udp::v4());

    Recorder recorder(ma_format_u8, 1);
    const int frame_size = recorder.GetFrameSize();

    while (true) {
        std::string server_ip;
        std::cout << "Enter server IP (or 'q' to quit): ";
        std::getline(std::cin, server_ip);

        if (server_ip == "q" || server_ip == "Q") {
            break;
        }

        std::cout << "Press Enter to record message..." << std::endl;
        std::string dummy;
        std::getline(std::cin, dummy);

        // Записываем звук
        auto rec_result = recorder.Record(65000, 1.5s);
        std::cout << "Recording done" << std::endl;

        // Вычисляем количество байт для отправки
        size_t bytes_to_send = rec_result.frames * frame_size;

        // Создаём endpoint сервера
        boost::system::error_code ec;
        auto endpoint = udp::endpoint(
            net::ip::make_address(server_ip, ec), port);

        if (ec) {
            std::cout << "Invalid IP address: " << ec.message() << std::endl;
            continue;
        }

        // Отправляем данные
        socket.send_to(
            net::buffer(rec_result.data.data(), bytes_to_send),
            endpoint, 0, ec);

        if (ec) {
            std::cout << "Error sending data: " << ec.message() << std::endl;
        }
        else {
            std::cout << "Sent " << bytes_to_send << " bytes to "
                << server_ip << ":" << port << std::endl;
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <client|server> <port>" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    uint16_t port = std::stoi(argv[2]);

    if (mode == "server") {
        StartServer(port);
    }
    else if (mode == "client") {
        StartClient(port);
    }
    else {
        std::cout << "Invalid mode. Use 'client' or 'server'" << std::endl;
        return 1;
    }

    return 0;
}