#pragma once

#include <spdlog/spdlog.h>

// 轻量日志封装：为后续 Diagnostics 模块扩展预留入口。
// 当前直接代理到 spdlog，保持零开销内联使用。

namespace genesis::diagnostics {

inline void set_level(spdlog::level::level_enum lvl) {
    spdlog::set_level(lvl);
}

inline void set_pattern(const char* pattern) {
    spdlog::set_pattern(pattern);
}

template <typename... Args>
inline void trace(const char* fmt, Args&&... args) {
    spdlog::trace(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void debug(const char* fmt, Args&&... args) {
    spdlog::debug(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void info(const char* fmt, Args&&... args) {
    spdlog::info(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void warn(const char* fmt, Args&&... args) {
    spdlog::warn(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void error(const char* fmt, Args&&... args) {
    spdlog::error(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void critical(const char* fmt, Args&&... args) {
    spdlog::critical(fmt, std::forward<Args>(args)...);
}

} // namespace genesis::diagnostics

