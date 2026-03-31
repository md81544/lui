#pragma once

// Ui is intended to be the main program interface.
// This controls output via the Terminal class
// and adds some higher-level functions for that
// e.g. string input.

#include "terminal.h"
#include "word_searcher.h"
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

struct TerminalSize {
    std::size_t rows;
    std::size_t cols;
};

class Ui final {
public:
    explicit Ui(std::string_view argv0);
    ~Ui() { };
    int run(); // main application run loop
private:
    void checkForTerminalResize();
    void resultsClear();
    void resultsSet(const std::vector<std::string>& vec);
    void displayHeader();
    void displayResults();
    void displayMenu();
    void restart();
    void hr(std::size_t row);
    void jumble();
    void lookup();

    terminal::Terminal m_term;
    TerminalSize m_termSize;
    std::unique_ptr<WordSearcher> m_ws;
    std::string m_searchString;
    std::string m_foundString;
    std::string m_clue;
    std::string m_comment;
    std::vector<std::string> m_results;
    std::size_t m_resultsScrollOffset { 0 };
    bool m_resultsScrollAtBottom { true };
};
