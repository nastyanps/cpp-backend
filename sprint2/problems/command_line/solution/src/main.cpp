#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <iostream>
#include <optional>
#include <thread>
#include "json_loader.h"
#include "request_handler.h"
#include "logging_request_handler.h"
#include "logger.h"
#include "application.h"
#include "ticker.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace json = boost::json;
namespace po = boost::program_options;

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

struct Args {
    std::optional<int> tick_period;
    std::string config_file;
    std::string www_root;
    bool randomize_spawn_points = false;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    po::options_description desc{"Allowed options"s};

    Args args;
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<int>()->value_name("milliseconds"), "set tick period")
        ("config-file,c", po::value(&args.config_file)->value_name("file"), "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir"), "set static files root")
        ("randomize-spawn-points", "spawn dogs at random positions");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        std::cout << desc;
        return std::nullopt;
    }

    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("Config file path is not specified"s);
    }
    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("Static files root path is not specified"s);
    }

    if (vm.contains("tick-period"s)) {
        args.tick_period = vm["tick-period"s].as<int>();
    }

    if (vm.contains("randomize-spawn-points"s)) {
        args.randomize_spawn_points = true;
    }

    return args;
}
}  // namespace

int main(int argc, const char* argv[]) {
    logging_json::InitLogging();

    try {
        auto args_opt = ParseCommandLine(argc, argv);
        if (!args_opt) {
            return EXIT_SUCCESS;
        }
        Args& args = *args_opt;

        model::Game game = json_loader::LoadGame(args.config_file, args.randomize_spawn_points);
        app::Application application{game};
        application.SetManualTickAllowed(!args.tick_period.has_value());

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        auto api_strand = net::make_strand(ioc);

        if (args.tick_period) {
            auto ticker = std::make_shared<Ticker>(
                api_strand, std::chrono::milliseconds(*args.tick_period),
                [&application](std::chrono::milliseconds delta) {
                    application.Tick(static_cast<double>(delta.count()) / 1000.0);
                });
            ticker->Start();
        }

        auto handler = std::make_shared<http_handler::RequestHandler>(
            application, std::filesystem::path(args.www_root), api_strand);

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
