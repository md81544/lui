#pragma once

#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace mgo {

// Usage:
// in main():
//        mgo::Log::init("app.log", Logger::Level::Info);
//        mgo::Log::info("Application started");
//        mgo::Log::warn("Low memory");
// elsewhere, just use:
//        mgo::Log::error("Crikey, dead serious error");
//        mgo::Log::debug("Got here");
//

class Log {
public:
    enum class Level {
        Debug,
        Info,
        Warn,
        Error
    };

    // Initialise the singleton with a log file. Must be called once before any
    // logging takes place — typically early in main(). Throws on failure.
    static void init(std::string_view filename, Level min_level = Level::Debug)
    {
        auto& instance = get_instance();
        std::scoped_lock lock { instance.m_mtx };

        if (instance.m_ofs.is_open()) {
            throw std::logic_error { "mgo::Log::init() called more than once" };
        }

        instance.m_ofs.open(filename.data(), std::ios::app);
        if (!instance.m_ofs) {
            throw std::runtime_error { std::format(
                "mgo::Log: cannot open '{}': {}", filename, std::strerror(errno)) };
        }

        instance.m_min_level = min_level;
    }

    // User-callable log functions
    static void
    debug(std::string_view msg, const std::source_location loc = std::source_location::current())
    {
        privateLog(Level::Debug, msg, loc);
    }
    static void
    info(std::string_view msg, const std::source_location loc = std::source_location::current())
    {
        privateLog(Level::Info, msg, loc);
    }
    static void
    warn(std::string_view msg, const std::source_location loc = std::source_location::current())
    {
        privateLog(Level::Warn, msg, loc);
    }
    static void
    error(std::string_view msg, const std::source_location loc = std::source_location::current())
    {
        privateLog(Level::Error, msg, loc);
    }

    // Non-copyable, non-movable.
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    Log(Log&&) = delete;
    Log& operator=(Log&&) = delete;

private:
    Log() = default;
    ~Log() = default;

    static void privateLog(
        Level level,
        std::string_view message,
        const std::source_location loc = std::source_location::current())
    {
        auto& instance = get_instance();
        std::scoped_lock lock { instance.m_mtx };

        if (level < instance.m_min_level) {
            return;
        }

        const auto entry = std::format(
            "[{}] [{}] [{}:{}] {}\n",
            current_timestamp(),
            level_label(level),
            loc.file_name(),
            loc.line(),
            message);

        if (instance.m_ofs.is_open()) {
            instance.m_ofs << entry;
            instance.m_ofs.flush();
        } else {
            std::cerr << entry;
        }
    }

    static Log& get_instance()
    {
        static Log instance;
        return instance;
    }

    static std::string current_timestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const auto ms
            = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        const std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm local {};
        localtime_r(&t, &local);
        return std::format(
            "{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}",
            local.tm_year + 1900,
            local.tm_mon + 1,
            local.tm_mday,
            local.tm_hour,
            local.tm_min,
            local.tm_sec,
            ms.count());
    }

    // NOTE! As of writing, clang++ doesn't support zoned_time.
    // When it catches up, a more c++2x version would be:
    //
    //  static std::string current_timestamp() {
    //      const auto now   = std::chrono::system_clock::now();
    //      const auto local = std::chrono::zoned_time{std::chrono::current_zone(),
    //                             std::chrono::floor<std::chrono::milliseconds>(now)};
    //      return std::format("{:%Y-%m-%dT%H:%M:%S}.{:03d}",
    //                         local,
    //                         (now.time_since_epoch() % std::chrono::seconds{1})
    //                             / std::chrono::milliseconds{1});
    //  }
    // ---- see note above -------------------------------------------------------------------

    static constexpr std::string_view level_label(Level level)
    {
        switch (level) {
            case Level::Debug:
                return "DEBUG";
            case Level::Info:
                return " INFO";
            case Level::Warn:
                return " WARN";
            case Level::Error:
                return "ERROR";
        }
        std::unreachable();
    }

    std::ofstream m_ofs;
    Level m_min_level { Level::Debug };
    std::mutex m_mtx;
};

} // namespace mgo
