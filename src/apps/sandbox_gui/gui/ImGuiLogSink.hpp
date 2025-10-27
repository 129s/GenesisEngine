#pragma once

#include <algorithm>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include <spdlog/sinks/base_sink.h>

namespace genesis::sandbox::gui
{

// Custom sink that buffers log lines for in-app display.
class ImGuiLogSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    explicit ImGuiLogSink(std::size_t maxEntries = 256)
        : max_entries_(std::max<std::size_t>(1, maxEntries))
    {
    }

    std::vector<std::string> snapshot()
    {
        std::lock_guard<std::mutex> lock(this->mutex_);
        return {entries_.begin(), entries_.end()};
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        std::string line(formatted.data(), formatted.size());
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        {
            line.pop_back();
        }

        entries_.push_back(std::move(line));
        if (entries_.size() > max_entries_)
        {
            entries_.pop_front();
        }
    }

    void flush_() override {}

private:
    std::size_t max_entries_;
    std::deque<std::string> entries_;
};

} // namespace genesis::sandbox::gui

