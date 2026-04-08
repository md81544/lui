#pragma once

#include <atomic>
#include <csignal>

// Usage: in main, call mgo::installSignalHandler()
// The signal hander should interrupt blocking calls
// (e.g. waiting for a key press). Low-level calls
// waiting for input will then return with n < 0 and errno
// set to EINTR. So low-level calls can throw, and
// higher level run loop code can check mgo::shutdown_requested
// (e.g. if (mgo::shutdown_requested.load(std::memory_order_relaxed)) { ... }

namespace mgo {

inline std::atomic<bool> shutdown_requested { false };

inline void signalHandler(int /*signal*/)
{
    shutdown_requested.store(true, std::memory_order_relaxed);
}

inline void installSignalHander() {
    // Not using std::signal here because std::signal
    // does not provide a parameter to unset SA_RESTART
    struct sigaction sa {};
    sa.sa_handler = mgo::signalHandler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

} // namespace mgo

