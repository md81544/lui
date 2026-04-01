#pragma once

// Ui is intended to be the main program interface.
// This controls output via the Terminal class
// and adds some higher-level functions for that
// e.g. string input.

#include "terminal.h"
#include "word_searcher.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace ui {

struct TerminalSize {
    std::size_t rows;
    std::size_t cols;
};

enum class InputMode {
    Append,
    Overwrite,
};

struct CurrentInput {
    bool active { false };
    std::string value;
    std::size_t cursorPos;
    std::size_t displayAtRow;
    std::size_t displayAtCol;
    std::size_t maxSize;
    bool upperCaseOnly { true };
    std::function<void()> callback; // called when user presses "Enter"
    InputMode inputMode;
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
    void displayCurrentInput();
    void displayResults();
    void displayMenu();
    void input(
        std::size_t row,
        std::size_t col,
        std::string defaultValue,
        std::function<void()> callback,
        std::size_t maxSize = 0,
        bool upperCase = true);
    // When input is active, keypresses are handled by this function:
    int inputHandleKeyPress(int key);
    void restart();
    void hr(std::size_t row);
    void jumble();
    void lookup();
    void log(std::string_view logEntry);
    std::filesystem::path locateDataDirectory(std::string_view argv0);

    terminal::Terminal m_term;
    TerminalSize m_termSize;
    std::unique_ptr<wordSearcher::WordSearcher> m_ws;
    std::string m_searchString;
    std::string m_foundString;
    std::string m_clue;
    std::string m_comment;
    std::vector<std::string> m_results;
    std::size_t m_resultsScrollOffset { 0 };
    bool m_resultsScrollAtBottom { true };
    CurrentInput m_currentInput;
    std::vector<std::string> m_debugLog;
};

} // namespace ui