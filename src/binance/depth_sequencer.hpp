#pragma once

#include "binance/types.hpp"

namespace titan::binance {

// binance futures rules: skip u < L, first update needs U <= L <= u, then pu == last u
class DepthSequencer {
public:
    enum class Verdict { Apply, Stale, Gap };

    void reset(SequenceId snapshot_last_id) noexcept {
        last_id_ = snapshot_last_id;
        bridged_ = false;
    }

    [[nodiscard]] Verdict check(const DepthUpdate& update) noexcept {
        if (!bridged_) {
            if (update.final_update_id < last_id_) {
                return Verdict::Stale;
            }
            if (update.first_update_id > last_id_) {
                return Verdict::Gap;
            }
            bridged_ = true;
        } else if (update.prev_final_update_id != last_id_) {
            return Verdict::Gap;
        }

        last_id_ = update.final_update_id;
        return Verdict::Apply;
    }

    [[nodiscard]] bool bridged() const noexcept { return bridged_; }
    [[nodiscard]] SequenceId last_id() const noexcept { return last_id_; }

private:
    SequenceId last_id_{0};
    bool bridged_{false};
};

}  // namespace titan::binance
