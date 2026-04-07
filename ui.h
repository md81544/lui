#pragma once

// Ui is intended to be the main program interface.
// This controls output via the Terminal class

#include "terminal.h"
#include "word_searcher.h"
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace ui {

struct TerminalSize {
    std::size_t rows;
    std::size_t cols;
};

class Ui final {
public:
    explicit Ui(std::string_view argv0, int wordComplexity);
    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;
    Ui( Ui&&) = delete;
    Ui& operator=(const Ui&&) = delete;
    ~Ui() { };
    int run(); // main application run loop
private:
    void checkForTerminalResize();
    void clearResults(terminal::OutputMode mode = terminal::OutputMode::render);
    void setResults(const std::vector<std::string>& vec);
    void displayHeader(terminal::OutputMode mode = terminal::OutputMode::render);
    void displayResults();
    void displayMenu();
    void restart();
    void hr(std::size_t row);
    void jumble();
    void lookup();
    void remove();
    void log(std::string_view logEntry);
    std::filesystem::path locateDataDirectory(std::string_view argv0);
    void enterFoundString();
    void enterSearchString();
    void enterCommentString();

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
    std::vector<std::string> m_debugLog;

    // The following define the layout of the output
    // Top row (the hr) of the results pane:
    static constexpr size_t m_resultsTopRow { 6 };
    // Number of rows from the bottom where the menu is placed:
    static constexpr size_t m_menuTopRowOffsetFromBottom { 4 };
    // Numner of rows to subtract from terminal height to get last row
    // of the results section:
    static constexpr size_t  m_menuResultsLastRowSubtract { 6 };

    // TODO: these SHOULD be
    // static constexpr size_t m_headerRowSize{6};
    // static constexpr size_t m_menuRowSize{4};
    // std::tuple<std::size_t, std:size_t> getResultsRowsTopBottom();
};

} // namespace ui