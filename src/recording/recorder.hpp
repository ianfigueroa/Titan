#pragma once

#include "core/types.hpp"
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace titan::recording {

/// Future: Feed recorder for capturing raw messages
/// Placeholder for replay support
class FeedRecorder {
public:
    /// Create a recorder (not yet implemented)
    /// @param output_path Path to output file
    explicit FeedRecorder(const std::filesystem::path& output_path);

    /// Record a raw message with timestamp
    /// @param raw_message Raw JSON message from WebSocket
    /// @param timestamp When message was received
    void record(std::string_view raw_message, Timestamp timestamp);

    /// Flush buffered data to disk
    void flush();

    /// Close the recorder
    void close();

private:
    std::filesystem::path output_path_;
    // Future: file handle, compression, batching
};

/// Future: Feed replayer for playback
class FeedReplayer {
public:
    /// Load a recording file
    /// @param input_path Path to recording file
    void load(const std::filesystem::path& input_path);

    /// Get next recorded message
    /// @return Message and timestamp, or nullopt if end of file
    std::optional<std::pair<std::string, Timestamp>> next();

    /// Reset to beginning of recording
    void reset();

    /// Get total number of messages
    [[nodiscard]] std::size_t message_count() const;

private:
    std::filesystem::path input_path_;
    // Future: file handle, index, current position
};

}  // namespace titan::recording
