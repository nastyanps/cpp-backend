#pragma once
#include "http_server.h"
#include "model.h"
#include "players.h"
#include <boost/json.hpp>
#include <optional>
#include <string_view>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using namespace std::literals;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

std::string MakeErrorBody(std::string_view code, std::string_view message);

StringResponse MakeJsonResponse(http::status status, std::string body, unsigned version, bool keep_alive,
                                  std::string_view allow = {});

class ApiHandler {
public:
    ApiHandler(model::Game& game, app::Players& players, app::PlayerTokens& tokens)
        : game_{game}
        , players_{players}
        , tokens_{tokens} {
    }

    StringResponse HandleApiRequest(const StringRequest& req);

private:
    StringResponse HandleMapsRequest(const StringRequest& req, std::string_view target);
    StringResponse HandleJoinRequest(const StringRequest& req);
    StringResponse HandlePlayersRequest(const StringRequest& req);
    std::optional<app::Token> TryExtractToken(const StringRequest& req);

    model::Game& game_;
    app::Players& players_;
    app::PlayerTokens& tokens_;
};

}  // namespace http_handler
