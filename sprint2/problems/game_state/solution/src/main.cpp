#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <thread>

#include "json_loader.h"
#include "request_handler.h"
#include "logging_request_handler.h"
#include "logger.h"
#include "application.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace json = boost::json;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}
}  // namespace

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: game_server <game-config-json> <static-files-dir>"sv << std::endl;
        return EXIT_FAILURE;
    }

    logging_json::InitLogging();

    try {
        model::Game game = json_loader::LoadGame(argv[1]);
        app::Application application{game};

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        auto api_strand = net::make_strand(ioc);

        auto handler = std::make_shared<http_handler::RequestHandler>(
            application, std::filesystem::path(argv[2]), api_strand);

        http_handler::LoggingRequestHandler logging_handler{*handler};

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        http_server::ServeHttp(ioc, {address, port},
            [&logging_handler](auto&& endpoint, auto&& req, auto&& send) {
                logging_handler(endpoint, std::forward<decltype(req)>(req),
                                 std::forward<decltype(send)>(send));
            });

        json::object start_data;
        start_data["port"] = port;
        start_data["address"] = address.to_string();
        logging_json::LogEvent("server started"sv, start_data);

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

        json::object exit_data;
        exit_data["code"] = 0;
        logging_json::LogEvent("server exited"sv, exit_data);

    } catch (const std::exception& ex) {
        json::object exit_data;
        exit_data["code"] = EXIT_FAILURE;
        exit_data["exception"] = ex.what();
        logging_json::LogEvent("server exited"sv, exit_data);
        return EXIT_FAILURE;
    }
}
