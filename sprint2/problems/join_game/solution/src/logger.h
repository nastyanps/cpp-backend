#pragma once
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/attributes.hpp>
#include <boost/json.hpp>
#include <boost/date_time.hpp>
#include <iostream>
#include <string_view>

namespace logging_json {

namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;
namespace json = boost::json;
using namespace std::literals;

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

inline void JsonFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    json::object obj;

    auto ts = rec[timestamp];
    obj["timestamp"] = to_iso_extended_string(*ts);

    auto data = rec[additional_data];
    obj["data"] = data ? *data : json::value(json::object{});

    auto msg = rec[expr::smessage];
    obj["message"] = msg ? *msg : std::string{};

    strm << json::serialize(obj);
}

inline void InitLogging() {
    logging::add_common_attributes();
    logging::add_console_log(
        std::cout,
        keywords::format = &JsonFormatter,
        keywords::auto_flush = true
    );
}

inline void LogEvent(std::string_view message, json::value data) {
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << message;
}

}  // namespace logging_json
