#pragma once

#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <string_view>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using namespace std::literals;



std::string MakeErrorBody(std::string_view code, std::string_view message);


http::response<http::string_body> MakeJsonResponse(http::status status,
    std::string body,
    unsigned version,
    bool keep_alive);

http::response<http::string_body> HandleApiRequest(
    const std::string& target,
    unsigned version,
    bool keep_alive,
    model::Game& game);

class RequestHandler {

public:

    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;

    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target(req.target());
        if (target.starts_with("/api/")) {
            send(HandleApiRequest(target, req.version(), req.keep_alive(), game_));
        } else {
            send(MakeJsonResponse(http::status::bad_request,
                                  MakeErrorBody("badRequest", "Bad request"),
                                  req.version(), req.keep_alive()));
        }
    }

private:

    model::Game& game_;

};

}  // namespace http_handler
