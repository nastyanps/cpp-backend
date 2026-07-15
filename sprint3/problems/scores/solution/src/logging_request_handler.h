#pragma once
#include "http_server.h"
#include "logger.h"
#include <boost/json.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <chrono>
#include <string>

namespace http_handler {

namespace json = boost::json;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;

template <typename SomeRequestHandler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(SomeRequestHandler& decorated)
        : decorated_{decorated} {
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(const tcp::endpoint& endpoint,
		     http::request<Body, http::basic_fields<Allocator>>&& req,
                     Send&& send) {
        const auto start_ts = std::chrono::steady_clock::now();
        const std::string ip = endpoint.address().to_string();

        LogRequest(ip, req);

        auto logging_send = [&send, ip, start_ts](auto&& response) {
            LogResponse(ip, response, start_ts);
            send(std::forward<decltype(response)>(response));
        };

        decorated_(endpoint, std::move(req), logging_send);
    }

private:
    template <typename Body, typename Allocator>
    static void LogRequest(const std::string& ip,
                            const http::request<Body, http::basic_fields<Allocator>>& req) {
        json::object data;
        data["ip"] = ip;
        data["URI"] = std::string(req.target());
        data["method"] = std::string(req.method_string());
        logging_json::LogEvent("request received"sv, data);
    }

    template <typename Body, typename Fields>
    static void LogResponse(const std::string& ip,
                             const http::response<Body, Fields>& response,
                             std::chrono::steady_clock::time_point start_ts) {
        const auto end_ts = std::chrono::steady_clock::now();
        const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_ts - start_ts).count();

        json::object data;
        data["ip"] = ip;
        data["response_time"] = static_cast<std::int64_t>(duration_ms);
        data["code"] = response.result_int();

        auto it = response.find(http::field::content_type);
        if (it != response.end()) {
            data["content_type"] = std::string(it->value());
        } else {
            data["content_type"] = nullptr;
        }
        logging_json::LogEvent("response sent"sv, data);
    }

    SomeRequestHandler& decorated_;
};

}  // namespace http_handler
