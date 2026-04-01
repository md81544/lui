#pragma once

#include <chrono>
#include <ctime>
#include <string>

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

} // namespace utils