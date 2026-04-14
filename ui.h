#pragma once

// Ui is intended to be the main program interface.
// This controls output via the Terminal class

#include "menu.h"

#include "terminal.h"
#include "word_searcher.h"
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string_view>
#include <vector>

namespace ui {

constexpr int MENU_JUMBLE = 1;
constexpr int MENU_REVERSE = 2;
constexpr int MENU_REGULAR = 3;
constexpr int MENU_THESAURUS = 4;
constexpr int MENU_LOOKUP = 5;
constexpr int MENU_DEFINE = 6;
constexpr int MENU_FILTER = 7;
constexpr int MENU_SAVE = 8;
constexpr int MENU_LOAD = 9;
constexpr int MENU_RESTART = 10;
constexpr int MENU_QUIT = 11;

class Ui final {
public:
    explicit Ui(std::string_view argv0, int wordComplexity);
    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;
    Ui(Ui&&) = delete;
    Ui& operator=(const Ui&&) = delete;
    ~Ui() { };
    int run(); // main application run loop
private:
    struct TerminalSize {
        std::size_t rows;
        std::size_t cols;
    };

    struct Clue {
        std::string searchString;
        std::string foundString;
        std::string clueNumber;
        std::string comment;
        bool dirty { false };
    };

    enum class ResultsType {
        FreeForm,
        Words,
    };

    struct Results {
        ResultsType type { ResultsType::FreeForm };
        std::size_t scrollOffset { 0 };
        bool scrollAtBottom { true };
        std::vector<std::string> vec;
        bool filtered { false };
    };

    void clearCommandPrompt();
    void checkForTerminalResize();
    bool checkTerminalLargeEnough();
    void restart();
    void clearResults(terminal::OutputMode mode = terminal::OutputMode::render);
    void setResults(const std::vector<std::string>& vec, ResultsType type = ResultsType::FreeForm);
    void setResults(std::string_view, ResultsType type = ResultsType::FreeForm);
    void appendResults(std::string_view, ResultsType type = ResultsType::FreeForm);
    void displayHeader(terminal::OutputMode mode = terminal::OutputMode::render);
    void displayResults(terminal::OutputMode mode = terminal::OutputMode::render);
    void displayMenu(terminal::OutputMode mode = terminal::OutputMode::render);
    void hr(std::size_t row, terminal::OutputMode mode = terminal::OutputMode::render);
    void jumble();
    void lookup();
    void regular();
    void reverse();
    void thesaurus();
    void define();
    void load();
    void save();
    void filterResults();
    void pageDownResults();
    void pageUpResults();
    void log(std::string_view logEntry);
    std::filesystem::path locateDataDirectory(std::string_view argv0);
    void enterFoundString();
    void enterFoundStringConstrained();
    void enterFoundStringUnconstrained();
    void enterSearchString();
    void enterCommentString();
    void enterClueNumber();

    terminal::Terminal m_term;
    TerminalSize m_termSize;
    Clue m_clue;
    std::unordered_map<std::string, Clue> m_savedClues;
    std::unique_ptr<wordSearcher::WordSearcher> m_ws;
    Results m_results;
    std::vector<std::string> m_debugLog;
    uint8_t m_commandSeqCount { 0 }; // used for two-key commands, e.g. :q

    std::list<int> m_outstandingKeyPresses;

    // UI layout
    static constexpr size_t m_headerRowSize { 6 };
    static constexpr size_t m_resultsTopRow { m_headerRowSize };
    static constexpr size_t m_menuRowSize { 4 };
    [[nodiscard]] std::size_t getResultsPaneRowSize();

    menu::Menu m_menu { m_term };
};

} // namespace ui