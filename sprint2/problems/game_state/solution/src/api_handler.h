#pragma once
#include "http_server.h"
#include "application.h"
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
    explicit ApiHandler(app::Application& application)
        : application_{application} {
    }

    StringResponse HandleApiRequest(const StringRequest& req);

private:
    StringResponse HandleMapsRequest(const StringRequest& req, std::string_view target);
    StringResponse HandleJoinRequest(const StringRequest& req);
    StringResponse HandlePlayersRequest(const StringRequest& req);
    StringResponse HandleStateRequest(const StringRequest& req);
    std::optional<app::Token> TryExtractToken(const StringRequest& req);

    app::Application& application_;
};

}  // namespace http_handler
