#include "trade/trade_flow.hpp"

namespace titan {

TradeFlow::TradeFlow(const Config::Engine& config)
    : vwap_(config.vwap_window)
    , alert_detector_(config.large_trade_std_devs)
{}

TradeFlowMetrics TradeFlow::process_trade(const binance::AggTrade& trade) {
    // Update VWAP
    vwap_.add_trade(trade.price, trade.quantity);

    // Track buy/sell volume
    // is_buyer_maker = true means the taker (aggressor) was a seller
    // is_buyer_maker = false means the taker (aggressor) was a buyer
    bool is_buy = !trade.is_buyer_maker;

    if (is_buy) {
        total_buy_volume_ += trade.quantity;
    } else {
        total_sell_volume_ += trade.quantity;
    }

    // Check for large trade alert
    auto alert = alert_detector_.check_trade(
        trade.price,
        trade.quantity,
        is_buy,
        vwap_.rolling_avg_size(),
        vwap_.rolling_std_dev()
    );

    if (alert.has_value()) {
        last_alert_ = alert;
    }

    return current_metrics();
}

TradeFlowMetrics TradeFlow::current_metrics() const {
    return TradeFlowMetrics{
        .vwap = vwap_.vwap(),
        .total_buy_volume = total_buy_volume_,
        .total_sell_volume = total_sell_volume_,
        .net_flow = total_buy_volume_ - total_sell_volume_,
        .trade_count = vwap_.trade_count(),
        .last_alert = last_alert_
    };
}

void TradeFlow::reset() {
    vwap_.clear();
    total_buy_volume_ = 0.0;
    total_sell_volume_ = 0.0;
    last_alert_.reset();
}

}  // namespace titan
