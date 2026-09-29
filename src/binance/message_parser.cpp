#include "binance/message_parser.hpp"
#include <simdjson.h>
#include <charconv>
#include <stdexcept>

namespace titan::binance {

namespace ondemand = simdjson::ondemand;

namespace {

// simdjson reads up to SIMDJSON_PADDING bytes past the end of the input
ondemand::document iterate(std::string_view json) {
    thread_local ondemand::parser parser;
    thread_local std::string buffer;
    buffer.reserve(json.size() + simdjson::SIMDJSON_PADDING);
    buffer.assign(json);
    return parser.iterate(buffer.data(), buffer.size(), buffer.capacity());
}

double parse_double(std::string_view s) {
    double value{};
    const char* end = s.data() + s.size();
    auto [ptr, ec] = std::from_chars(s.data(), end, value);
    if (ec != std::errc{} || ptr != end) {
        throw std::invalid_argument("bad number: " + std::string(s));
    }
    return value;
}

std::vector<PriceLevel> parse_price_levels(ondemand::array arr) {
    std::vector<PriceLevel> levels;

    for (auto level : arr) {
        ondemand::array pair;
        if (level.get_array().get(pair) != simdjson::SUCCESS) {
            continue;
        }
        std::string_view price;
        std::string_view qty;
        std::size_t n = 0;
        for (auto field : pair) {
            std::string_view text = field.get_string();
            if (n == 0) price = text;
            else if (n == 1) qty = text;
            ++n;
        }
        if (n < 2) {
            continue;
        }
        levels.emplace_back(FixedPrice::parse(price), parse_double(qty));
    }

    return levels;
}

DepthUpdate depth_update_from(ondemand::object obj) {
    DepthUpdate update;
    update.event_type = std::string(std::string_view(obj["e"]));
    update.event_time = obj["E"].get_uint64();
    std::uint64_t transaction_time{};
    update.transaction_time =
        obj["T"].get_uint64().get(transaction_time) == simdjson::SUCCESS
            ? transaction_time : update.event_time;
    update.symbol = std::string(std::string_view(obj["s"]));
    update.first_update_id = obj["U"].get_uint64();
    update.final_update_id = obj["u"].get_uint64();
    update.prev_final_update_id = obj["pu"].get_uint64();
    update.bids = parse_price_levels(obj["b"].get_array());
    update.asks = parse_price_levels(obj["a"].get_array());
    return update;
}

AggTrade agg_trade_from(ondemand::object obj) {
    AggTrade trade;
    trade.event_type = std::string(std::string_view(obj["e"]));
    trade.event_time = obj["E"].get_uint64();
    trade.agg_trade_id = obj["a"].get_uint64();
    trade.symbol = std::string(std::string_view(obj["s"]));
    trade.price = parse_double(obj["p"].get_string());
    trade.quantity = parse_double(obj["q"].get_string());
    trade.first_trade_id = obj["f"].get_uint64();
    trade.last_trade_id = obj["l"].get_uint64();
    trade.trade_time = obj["T"].get_uint64();
    trade.is_buyer_maker = obj["m"].get_bool();
    return trade;
}

template <typename T, typename Fn>
Result<T, std::string> guarded(Fn&& fn) {
    try {
        return Result<T, std::string>::Ok(fn());
    } catch (const simdjson::simdjson_error& e) {
        return Result<T, std::string>::Err(std::string("JSON parse error: ") + e.what());
    } catch (const std::exception& e) {
        return Result<T, std::string>::Err(std::string("Parse error: ") + e.what());
    }
}

}  // namespace

Result<DepthUpdate, std::string> MessageParser::parse_depth_update(std::string_view json_str) {
    return guarded<DepthUpdate>([&] {
        auto doc = iterate(json_str);
        return depth_update_from(doc.get_object());
    });
}

Result<AggTrade, std::string> MessageParser::parse_agg_trade(std::string_view json_str) {
    return guarded<AggTrade>([&] {
        auto doc = iterate(json_str);
        return agg_trade_from(doc.get_object());
    });
}

Result<DepthSnapshot, std::string> MessageParser::parse_depth_snapshot(
    std::string_view json_str,
    std::string_view symbol
) {
    return guarded<DepthSnapshot>([&] {
        auto doc = iterate(json_str);
        ondemand::object obj = doc.get_object();

        DepthSnapshot snapshot;
        snapshot.last_update_id = obj["lastUpdateId"].get_uint64();
        std::uint64_t event_time{};
        snapshot.event_time =
            obj["E"].get_uint64().get(event_time) == simdjson::SUCCESS ? event_time : 0;
        snapshot.symbol = std::string(symbol);
        snapshot.bids = parse_price_levels(obj["bids"].get_array());
        snapshot.asks = parse_price_levels(obj["asks"].get_array());
        return snapshot;
    });
}

Result<StreamEvent, std::string> MessageParser::parse_stream_event(std::string_view json_str) {
    return guarded<StreamEvent>([&]() -> StreamEvent {
        auto doc = iterate(json_str);
        ondemand::object obj = doc.get_object();

        std::string_view stream = obj["stream"];
        if (is_depth_stream(stream)) {
            return depth_update_from(obj["data"].get_object());
        }
        if (is_agg_trade_stream(stream)) {
            return agg_trade_from(obj["data"].get_object());
        }
        return std::monostate{};
    });
}

Result<StreamMessage, std::string> MessageParser::parse_combined_stream(std::string_view json_str) {
    return guarded<StreamMessage>([&] {
        auto doc = iterate(json_str);
        ondemand::object obj = doc.get_object();

        StreamMessage msg;
        msg.stream = std::string(std::string_view(obj["stream"]));
        msg.data = std::string(std::string_view(obj["data"].raw_json()));
        return msg;
    });
}

bool MessageParser::is_depth_stream(std::string_view stream_name) {
    return stream_name.find("@depth") != std::string_view::npos;
}

bool MessageParser::is_agg_trade_stream(std::string_view stream_name) {
    return stream_name.find("@aggTrade") != std::string_view::npos;
}

}  // namespace titan::binance
