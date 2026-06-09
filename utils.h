#pragma once

#include "ascii.h"
#include "ui.h"

#include <charconv>
#include <chrono>
#include <concepts>
#include <ctime>
#include <optional>
#include <ranges>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

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

/// Return every nth character from a string
inline std::string everyNth(std::string_view s, std::size_t n)
{
    // Could use std::views::stride() here but as of writing my
    // version of clang++ doesn't support it
    auto indices = std::views::iota(std::size_t { 0 }, s.size())
        | std::views::filter([n](std::size_t i) { return i % n == 0; });
    auto chars = indices | std::views::transform([&s](std::size_t i) { return s[i]; });
    return std::string(chars.begin(), chars.end());
}

/// Remove any ANSI Escape sequences from a string
inline std::string stripAnsi(const std::string& input)
{
    static const std::regex rgx(R"(\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~]))");
    return std::regex_replace(input, rgx, "");
}

// NB, alters the string passed in
inline void trim(std::string& s)
{
    auto not_space = [](unsigned char c) { return !ascii::isspace(c); };
    s.erase(std::ranges::find_if(s | std::views::reverse, not_space).base(), s.end());
    s.erase(s.begin(), std::ranges::find_if(s, not_space));
}

inline std::vector<std::string> split(std::string_view sv, char sep)
{
    std::string s { sv };
    std::vector<std::string> vec;
    for (auto subrange : s | std::views::split(sep)) {
        std::string tok(subrange.begin(), subrange.end());
        trim(tok);
        vec.push_back(tok);
    }
    return vec;
}

template <std::integral T> std::optional<T> parseNumber(std::string_view sv)
{
    auto start = sv.find_first_not_of(" \t\r\n\v\f");
    if (start == std::string_view::npos) {
        return std::nullopt;
    }
    sv.remove_prefix(start);

    T result {};
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result);
    if (ec != std::errc {}) {
        return std::nullopt;
    }
    return result;
}

template <std::floating_point T>
std::optional<T>
parseNumber(std::string_view sv, std::chars_format fmt = std::chars_format::general)
{
    auto start = sv.find_first_not_of(" \t\r\n\v\f");
    if (start == std::string_view::npos) {
        return std::nullopt;
    }
    sv.remove_prefix(start);

    T result {};
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result, fmt);
    if (ec != std::errc {}) {
        return std::nullopt;
    }
    return result;
}

// Convert Command.data from string to size_t; returns zero if missing
inline std::size_t dataToCol(const ui::Command& cmd)
{
    std::size_t col = 0;
    if (!cmd.data.empty()) {
        auto optCol = utils::parseNumber<std::size_t>(cmd.data);
        if (optCol.has_value()) {
            col = *optCol;
        }
    }
    return col;
}

} // namespace utils
