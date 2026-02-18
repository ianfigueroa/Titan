#include "binance/message_parser.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace titan::binance {

using json = nlohmann::json;

namespace {

/// Parse price level array: ["price", "quantity"]
/// Uses FixedPrice::parse for exact precision, stod for quantity
std::vector<PriceLevel> parse_price_levels(const json& arr) {
    std::vector<PriceLevel> levels;
    levels.reserve(arr.size());

    for (const auto& level : arr) {
        if (!level.is_array() || level.size() < 2) {
            continue;
        }
        // Parse price as fixed-point for exact map key matching
        FixedPrice price = FixedPrice::parse(level[0].get<std::string>());
        // Quantity can stay as double for accumulation
        Quantity qty = std::stod(level[1].get<std::string>());
        levels.emplace_back(price, qty);
    }

    return levels;
}

}  // namespace

Result<DepthUpdate, std::string> MessageParser::parse_depth_update(std::string_view json_str) {
    try {
        auto j = json::parse(json_str);

        // Validate required fields
        if (!j.contains("e") || !j.contains("E") || !j.contains("s") ||
            !j.contains("U") || !j.contains("u") || !j.contains("pu") ||
            !j.contains("b") || !j.contains("a")) {
            return Result<DepthUpdate, std::string>::Err("Missing required fields in depth update");
        }

        DepthUpdate update;
        update.event_type = j["e"].get<std::string>();
        update.event_time = j["E"].get<std::uint64_t>();
        update.transaction_time = j.value("T", update.event_time);
        update.symbol = j["s"].get<std::string>();
        update.first_update_id = j["U"].get<SequenceId>();
        update.final_update_id = j["u"].get<SequenceId>();
        update.prev_final_update_id = j["pu"].get<SequenceId>();
        update.bids = parse_price_levels(j["b"]);
        update.asks = parse_price_levels(j["a"]);

        return Result<DepthUpdate, std::string>::Ok(std::move(update));

    } catch (const json::exception& e) {
        return Result<DepthUpdate, std::string>::Err(
            std::string("JSON parse error: ") + e.what()
        );
    } catch (const std::exception& e) {
        return Result<DepthUpdate, std::string>::Err(
            std::string("Parse error: ") + e.what()
        );
    }
}

Result<AggTrade, std::string> MessageParser::parse_agg_trade(std::string_view json_str) {
    try {
        auto j = json::parse(json_str);

        // Validate required fields
        if (!j.contains("e") || !j.contains("E") || !j.contains("s") ||
            !j.contains("a") || !j.contains("p") || !j.contains("q") ||
            !j.contains("f") || !j.contains("l") || !j.contains("T") ||
            !j.contains("m")) {
            return Result<AggTrade, std::string>::Err("Missing required fields in aggTrade");
        }

        AggTrade trade;
        trade.event_type = j["e"].get<std::string>();
        trade.event_time = j["E"].get<std::uint64_t>();
        trade.symbol = j["s"].get<std::string>();
        trade.agg_trade_id = j["a"].get<TradeId>();
        trade.price = std::stod(j["p"].get<std::string>());
        trade.quantity = std::stod(j["q"].get<std::string>());
        trade.first_trade_id = j["f"].get<TradeId>();
        trade.last_trade_id = j["l"].get<TradeId>();
        trade.trade_time = j["T"].get<std::uint64_t>();
        trade.is_buyer_maker = j["m"].get<bool>();

        return Result<AggTrade, std::string>::Ok(std::move(trade));

    } catch (const json::exception& e) {
        return Result<AggTrade, std::string>::Err(
            std::string("JSON parse error: ") + e.what()
        );
    } catch (const std::exception& e) {
        return Result<AggTrade, std::string>::Err(
            std::string("Parse error: ") + e.what()
        );
    }
}

Result<DepthSnapshot, std::string> MessageParser::parse_depth_snapshot(
    std::string_view json_str,
    std::string_view symbol
) {
    try {
        auto j = json::parse(json_str);

        // Validate required fields
        if (!j.contains("lastUpdateId") || !j.contains("bids") || !j.contains("asks")) {
            return Result<DepthSnapshot, std::string>::Err("Missing required fields in depth snapshot");
        }

        DepthSnapshot snapshot;
        snapshot.last_update_id = j["lastUpdateId"].get<SequenceId>();
        snapshot.event_time = j.value("E", 0ULL);
        snapshot.symbol = std::string(symbol);
        snapshot.bids = parse_price_levels(j["bids"]);
        snapshot.asks = parse_price_levels(j["asks"]);

        return Result<DepthSnapshot, std::string>::Ok(std::move(snapshot));

    } catch (const json::exception& e) {
        return Result<DepthSnapshot, std::string>::Err(
            std::string("JSON parse error: ") + e.what()
        );
    } catch (const std::exception& e) {
        return Result<DepthSnapshot, std::string>::Err(
            std::string("Parse error: ") + e.what()
        );
    }
}

Result<StreamMessage, std::string> MessageParser::parse_combined_stream(std::string_view json_str) {
    try {
        auto j = json::parse(json_str);

        if (!j.contains("stream") || !j.contains("data")) {
            return Result<StreamMessage, std::string>::Err("Missing stream or data field");
        }

        StreamMessage msg;
        msg.stream = j["stream"].get<std::string>();
        msg.data = j["data"].dump();

        return Result<StreamMessage, std::string>::Ok(std::move(msg));

    } catch (const json::exception& e) {
        return Result<StreamMessage, std::string>::Err(
            std::string("JSON parse error: ") + e.what()
        );
    }
}

bool MessageParser::is_depth_stream(std::string_view stream_name) {
    return stream_name.find("@depth") != std::string_view::npos;
}

bool MessageParser::is_agg_trade_stream(std::string_view stream_name) {
    return stream_name.find("@aggTrade") != std::string_view::npos;
}

}  // namespace titan::binance
