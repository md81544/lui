#pragma once

#include <chrono>
#include <ctime>
#include <ranges>
#include <string>
#include <string_view>

namespace utils {

inline std::string currentTimeString()
{
    // Required because clang doesn't (as of writing)
    // support chrono's zoned_time, which would mean
    // we'd get a time without DST applied if using
    // plain chrono system_clock
    const auto now = std::chrono::system_clock::now();
    const auto ms
        = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm {};
    localtime_r(&t, &tm); // applies local timezone + DST

    return std::format("{:02}:{:02}:{:02}.{:03}", tm.tm_hour, tm.tm_min, tm.tm_sec, ms.count());
}

inline std::string everyNth(std::string_view s, std::size_t n)
{
    // Could use std::views::stride() here but as of writing my
    // version of clang++ doesn't support it
    auto indices = std::views::iota(std::size_t { 0 }, s.size())
        | std::views::filter([n](std::size_t i) { return i % n == 0; });
    auto chars = indices | std::views::transform([&s](std::size_t i) { return s[i]; });
    return std::string(chars.begin(), chars.end());
}

} // namespace utils