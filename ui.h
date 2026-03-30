#pragma once

// Ui is intended to be the main program interface.
// This controls output via the Terminal class
// and adds some higher-level functions for that
// e.g. string input.

#include "terminal.h"
#include "word_searcher.h"
#include <memory>
#include <string_view>

class Ui final {
public:
    explicit Ui(std::string_view argv0);
    ~Ui() { };
    int run(); // main application run loop
private:
    terminal::Terminal m_term;
    std::unique_ptr<WordSearcher> m_ws;
};
