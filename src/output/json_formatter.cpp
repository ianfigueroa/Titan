#include "output/json_formatter.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace titan::output {

nlohmann::json JsonFormatter::format_metrics(
    const BookSnapshot& book,
    const TradeFlowMetrics& flow
) {
    return nlohmann::json{
        {"type", "metrics"},
        {"timestamp", iso_timestamp()},
        {"book", {
            {"bestBid", book.best_bid},
            {"bestBidQty", book.best_bid_qty},
            {"bestAsk", book.best_ask},
            {"bestAskQty", book.best_ask_qty},
            {"spread", book.spread},
            {"spreadBps", book.spread_bps},
            {"midPrice", book.mid_price},
            {"imbalance", book.imbalance},
            {"lastUpdateId", book.last_update_id}
        }},
        {"trade", {
            {"vwap", flow.vwap},
            {"buyVolume", flow.total_buy_volume},
            {"sellVolume", flow.total_sell_volume},
            {"netFlow", flow.net_flow},
            {"tradeCount", flow.trade_count}
        }}
    };
}

nlohmann::json JsonFormatter::format_alert(const TradeAlert& alert) {
    return nlohmann::json{
        {"type", "alert"},
        {"timestamp", iso_timestamp()},
        {"side", alert.is_buy ? "BUY" : "SELL"},
        {"price", alert.price},
        {"quantity", alert.quantity},
        {"deviation", alert.deviation}
    };
}

nlohmann::json JsonFormatter::format_status(
    bool connected,
    const std::string& state
) {
    return nlohmann::json{
        {"type", "status"},
        {"timestamp", iso_timestamp()},
        {"connected", connected},
        {"state", state}
    };
}

std::string JsonFormatter::iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

}  // namespace titan::output
