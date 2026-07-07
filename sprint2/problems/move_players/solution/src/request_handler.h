#pragma once
#include "http_server.h"
#include "api_handler.h"
#include "application.h"

#include <boost/asio/strand.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <filesystem>
#include <memory>
#include <unordered_map>

namespace http_handler {

namespace net = boost::asio;
namespace fs = std::filesystem;
namespace sys = boost::system;

std::string UrlDecode(std::string_view str);
std::string_view MimeTypeByExtension(std::string ext);
bool IsSubPath(fs::path path, fs::path base);

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using tcp = net::ip::tcp;

    RequestHandler(app::Application& application, fs::path static_root, Strand api_strand)
        : static_root_{fs::weakly_canonical(static_root)}
        , api_strand_{api_strand}
        , api_handler_{std::make_shared<ApiHandler>(application)} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(const tcp::endpoint& endpoint,
                     http::request<Body, http::basic_fields<Allocator>>&& req,
                     Send&& send) {
        std::string target(req.target());

        if (target.starts_with("/api/")) {
            auto handle = [self = shared_from_this(), send, req = std::move(req)]() mutable {
                try {
                    assert(self->api_strand_.running_in_this_thread());
                    send(self->api_handler_->HandleApiRequest(req));
                } catch (...) {
                    send(MakeJsonResponse(http::status::internal_server_error,
                                           MakeErrorBody("internalError", "Internal server error"),
                                           req.version(), req.keep_alive()));
                }
            };
            return net::dispatch(api_strand_, handle);
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
            send(MakeJsonResponse(http::status::method_not_allowed,
                                   MakeErrorBody("invalidMethod", "Only GET and HEAD are expected"),
                                   version, keep_alive));
            return;
        }

        std::string decoded_target = UrlDecode(std::string_view(req.target()));
        fs::path rel_path = fs::path(decoded_target).relative_path();
        fs::path file_path = fs::weakly_canonical(static_root_ / rel_path);

        if (!IsSubPath(file_path, static_root_)) {
            send(MakeJsonResponse(http::status::bad_request,
                                   MakeErrorBody("badRequest", "Bad request"), version, keep_alive));
            return;
        }

        sys::error_code fs_ec;
        if (fs::is_directory(file_path, fs_ec)) {
            file_path /= "index.html";
        }

        if (!fs::exists(file_path, fs_ec) || !fs::is_regular_file(file_path, fs_ec)) {
            send(MakeJsonResponse(http::status::not_found,
                                   MakeErrorBody("fileNotFound", "File not found"), version, keep_alive));
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
            send(MakeJsonResponse(http::status::not_found,
                                   MakeErrorBody("fileNotFound", "File not found"), version, keep_alive));
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

    fs::path static_root_;
    Strand api_strand_;
    std::shared_ptr<ApiHandler> api_handler_;
};

}  // namespace http_handler
