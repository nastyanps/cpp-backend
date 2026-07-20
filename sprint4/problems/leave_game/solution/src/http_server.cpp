#include "http_server.h"
#include "logger.h"

#include <boost/asio/dispatch.hpp>

namespace http_server {

namespace json = boost::json;
using namespace std::literals;

void ReportError(beast::error_code ec, std::string_view what) {
    json::object data;
    data["code"] = ec.value();
    data["text"] = ec.message();
    data["where"] = std::string(what);
    logging_json::LogEvent("error"sv, data);
}

}  // namespace http_server
