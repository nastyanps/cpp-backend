#pragma once

#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <string_view>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;
namespace sys = boost::system;

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

std::string UrlDecode(std::string_view str);
std::string_view MimeTypeByExtension(std::string ext);
bool IsSubPath(fs::path path, fs::path base);


class RequestHandler {

public:

    RequestHandler(model::Game& game, fs::path static_root)
        : game_{game}
        , static_root_{fs::weakly_canonical(static_root)} {
    }

    RequestHandler(const RequestHandler&) = delete;

    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target(req.target());
        if (target.starts_with("/api/")) {
            send(HandleApiRequest(target, req.version(), req.keep_alive(), game_));
	    return;
        }
	HandleStaticRequest(std::move(req), std::forward<Send>(send));
    }

private:

    template <typename Body, typename Allocator, typename Send>
    void HandleStaticRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        const auto method = req.method();
        const auto version = req.version();
        const auto keep_alive = req.keep_alive();

        if (method != http::verb::get && method != http::verb::head) {
            send(MakeTextResponse(http::status::method_not_allowed,
                                   "Only GET and HEAD are expected"sv, version, keep_alive));
            return;
        }

        std::string decoded_target = UrlDecode(std::string_view(req.target()));
        fs::path rel_path = fs::path(decoded_target).relative_path();
        fs::path file_path = fs::weakly_canonical(static_root_ / rel_path);

        if (!IsSubPath(file_path, static_root_)) {
            send(MakeTextResponse(http::status::bad_request,
                                   "Bad request: path is outside static root"sv, version, keep_alive));
            return;
        }

        sys::error_code fs_ec;
        if (fs::is_directory(file_path, fs_ec)) {
            file_path /= "index.html";
        }

        if (!fs::exists(file_path, fs_ec) || !fs::is_regular_file(file_path, fs_ec)) {
            send(MakeTextResponse(http::status::not_found, "File not found"sv, version, keep_alive));
            return;
        }

        std::string_view content_type = MimeTypeByExtension(file_path.extension().string());

        if (method == http::verb::head) {
            http::response<http::string_body> response(http::status::ok, version);
            response.set(http::field::content_type, content_type);
            response.content_length(fs::file_size(file_path));
            response.keep_alive(keep_alive);
            send(std::move(response));
            return;
        }

        http::file_body::value_type file;
        sys::error_code ec;
        file.open(file_path.string().c_str(), beast::file_mode::read, ec);
        if (ec) {
            send(MakeTextResponse(http::status::not_found, "File not found"sv, version, keep_alive));
            return;
        }

        http::response<http::file_body> response;
        response.version(version);
        response.result(http::status::ok);
        response.set(http::field::content_type, content_type);
        response.body() = std::move(file);
        response.prepare_payload();
        response.keep_alive(keep_alive);
        send(std::move(response));
    }

    model::Game& game_;
    fs::path static_root_;

};

}  // namespace http_handler
