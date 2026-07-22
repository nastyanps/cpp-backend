#pragma once
#include "http_server.h"
#include "application.h"
#include "extra_data.h"
#include "postgres/database.h"

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
    ApiHandler(app::Application& application, extra_data::LootTypesData& extra_data,
               postgres::Database& db)
        : application_{application}
        , extra_data_{extra_data}
        , db_{db} {
    }

    StringResponse HandleApiRequest(const StringRequest& req);

private:
    StringResponse HandleMapsRequest(const StringRequest& req, std::string_view target);
    StringResponse HandleJoinRequest(const StringRequest& req);
    StringResponse HandlePlayersRequest(const StringRequest& req);
    StringResponse HandleStateRequest(const StringRequest& req);
    StringResponse HandleActionRequest(const StringRequest& req);
    StringResponse HandleTickRequest(const StringRequest& req);
    StringResponse HandleRecordsRequest(const StringRequest& req, std::string_view target);
    static std::optional<app::Token> TryExtractToken(const StringRequest& req);

    template <typename Fn>
    StringResponse ExecuteAuthorized(const StringRequest& req, Fn&& action) {
        unsigned version = req.version();
        bool keep_alive = req.keep_alive();

        auto token_opt = TryExtractToken(req);
        if (!token_opt) {
            return MakeJsonResponse(http::status::unauthorized,
                                     MakeErrorBody("invalidToken", "Authorization header is required"),
                                     version, keep_alive);
        }

        app::Player* player = application_.FindPlayerByToken(*token_opt);
        if (!player) {
            return MakeJsonResponse(http::status::unauthorized,
                                     MakeErrorBody("unknownToken", "Player token has not been found"),
                                     version, keep_alive);
        }

        return action(*player);
    }

private:
    app::Application& application_;
    extra_data::LootTypesData& extra_data_;
    postgres::Database& db_;
};

}  // namespace http_handler
