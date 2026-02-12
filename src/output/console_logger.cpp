#include "output/console_logger.hpp"
#include <spdlog/spdlog.h>
#include <iomanip>
#include <sstream>

namespace titan::output {

ConsoleLogger::ConsoleLogger(std::chrono::milliseconds interval)
    : interval_(interval)
    , last_output_(std::chrono::steady_clock::now() - interval)  // Allow immediate first log
{}

bool ConsoleLogger::log_metrics(const BookSnapshot& book, const TradeFlowMetrics& flow) {
    auto now = std::chrono::steady_clock::now();

    if (!force_next_ && (now - last_output_) < interval_) {
        return false;
    }

    force_next_ = false;
    last_output_ = now;

    // Format imbalance as percentage
    std::ostringstream imb_str;
    imb_str << std::showpos << std::fixed << std::setprecision(0)
            << (book.imbalance * 100) << "%";

    // Format: BID: price (qty) | ASK: price (qty) | SPREAD: X bps | IMB: X% | VWAP: X | TRADES/s: X
    spdlog::info(
        "BID: {:.2f} ({:.3f}) | ASK: {:.2f} ({:.3f}) | SPREAD: {:.1f}bps | "
        "IMB: {} | VWAP: {:.2f} | TRADES: {}",
        book.best_bid, book.best_bid_qty,
        book.best_ask, book.best_ask_qty,
        book.spread_bps,
        imb_str.str(),
        flow.vwap,
        flow.trade_count
    );

    return true;
}

void ConsoleLogger::log_alert(const TradeAlert& alert) {
    std::string side = alert.is_buy ? "BUY" : "SELL";

    spdlog::warn(
        "ALERT: LARGE {} {:.3f} BTC @ {:.2f} ({:.1f} sigma)",
        side,
        alert.quantity,
        alert.price,
        alert.deviation
    );
}

void ConsoleLogger::log_connection_status(bool connected, const std::string& details) {
    if (connected) {
        spdlog::info("Connection established{}", details.empty() ? "" : ": " + details);
    } else {
        spdlog::warn("Connection lost{}", details.empty() ? "" : ": " + details);
    }
}

void ConsoleLogger::log_sync_status(const std::string& status) {
    spdlog::info("Sync: {}", status);
}

void ConsoleLogger::force_next() {
    force_next_ = true;
}

}  // namespace titan::output
