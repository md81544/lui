#include "ui.h"
#include "ascii.h"
#include "configreader.h"
#include "hotkeys.h"
#include "keypress.h"
#include "log.h" // IWYU pragma: keep
#include "menu.h"
#include "signal_handler.h"
#include "terminal.h"
#include "utils.h"
#include "word_searcher.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <numbers>
#include <random>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

enum class MenuItem {
    Jumble,
    Reverse,
    Regular,
    Thesaurus,
    Lookup,
    Define,
    Filter,
    Done,
    Save,
    Load,
    Restart,
    Quit,
};

std::vector<std::string> lettersInACircle(std::string_view letters)
{
    std::string localLetters { letters };
    if (localLetters.size() % 2 == 1) {
        // We need a even number of characters
        localLetters.push_back(' ');
    }
    const std::size_t len = localLetters.size();
    const std::size_t radius = static_cast<std::size_t>(
        std::ceil(std::sqrt(static_cast<float>(len) / std::numbers::pi_v<float>)));

    const std::size_t rows = radius * 2 + 1;
    const std::size_t cols = radius * 4 + 1;
    std::vector<std::string> grid(rows, std::string(cols, ' '));

    for (std::size_t i = 0; i < len / 2; ++i) {
        const std::size_t halfLength = len / 2;
        const float angle
            = (static_cast<float>(i) / static_cast<float>(halfLength)) * std::numbers::pi_v<float>;

        const auto x1 = static_cast<std::ptrdiff_t>(std::round(std::cos(angle) * radius));
        const auto y1 = static_cast<std::ptrdiff_t>(std::round(std::sin(angle) * radius));
        const auto x2 = -x1;
        const auto y2 = -y1;

        const std::size_t row1 = static_cast<std::size_t>(y1 + static_cast<std::ptrdiff_t>(radius));
        const std::size_t col1
            = static_cast<std::size_t>(x1 * 2 + static_cast<std::ptrdiff_t>(radius) * 2);
        const std::size_t row2 = static_cast<std::size_t>(y2 + static_cast<std::ptrdiff_t>(radius));
        const std::size_t col2
            = static_cast<std::size_t>(x2 * 2 + static_cast<std::ptrdiff_t>(radius) * 2);

        grid[row1][col1]
            = static_cast<char>(ascii::toupper(static_cast<unsigned char>(localLetters[i * 2])));
        grid[row2][col2] = static_cast<char>(
            ascii::toupper(static_cast<unsigned char>(localLetters[i * 2 + 1])));
    }

    return grid;
}

std::size_t separatedStringSize(std::string_view foundString)
{
    // Returns size of string minus any word separators ('/')
    return std::count_if(foundString.begin(), foundString.end(), [](char c) { return c != '/'; });
}

std::string
letterCount(std::string_view searchString, [[maybe_unused]] std::string_view foundString)
{
    // Returns a string in the format "5 letters" say. If foundString contains
    // a word separator (or multiple), then the output is, for example, "3,2 letters"
    if (!foundString.contains('/')) {
        return std::format("{} letters", searchString.size());
    }

    std::string rc;
    for (auto subrange : foundString | std::views::split('/')) {
        if (!rc.empty()) {
            rc.push_back(',');
        }
        rc.append(std::to_string(subrange.end() - subrange.begin()));
    }
    rc.append(" letters");
    return rc;
}

} // anonymous namespace

namespace ui {

Ui::Ui(std::string_view argv0)
{
    const std::filesystem::path dataDir = locateDataDirectory(argv0);
    m_cfg = std::make_unique<mgo::ConfigReader>((dataDir / "lui.yaml").c_str());
    bool enableFocusReporting { true };
#ifdef NDEBUG
    enableFocusReporting = m_cfg->readBool("release/focusReporting", true);
#else
    enableFocusReporting = m_cfg->readBool("debug/focusReporting", true);
#endif
    if (enableFocusReporting) {
        log("Focus reporting ON");
    } else {
        log("Focus reporting OFF");
    }
    m_term = std::make_unique<terminal::Terminal>(enableFocusReporting);
    if (!checkTerminalLargeEnough()) {
        throw(std::runtime_error("Terminal size is too small!"));
    }
    m_menu = std::make_unique<menu::Menu>(*m_term);
    log("DEBUG LOG");
    // What does the terminal actually support?
    terminal::ColourDepth actualColourDepth = m_term->detectColourDepth();
    terminal::ColourDepth colourDepth = actualColourDepth;
    std::string cd = m_cfg->readString("colourDepth", "trueColour");
    if (cd == "mono") {
        colourDepth = terminal::ColourDepth::None;
    } else if (cd == "ansi16") {
        if (actualColourDepth >= terminal::ColourDepth::Ansi16) {
            colourDepth = terminal::ColourDepth::Ansi16;
        }
    } else if (cd == "ansi256") {
        if (actualColourDepth >= terminal::ColourDepth::Ansi256) {
            colourDepth = terminal::ColourDepth::Ansi256;
        }
    } else if (cd == "trueColour") {
        if (actualColourDepth >= terminal::ColourDepth::TrueColour) {
            colourDepth = terminal::ColourDepth::TrueColour;
        }
    } else {
        log("Unknown colourDepth specified in config.");
        colourDepth = terminal::ColourDepth::None;
    }
    m_term->setColourDepth(colourDepth);
    switch (colourDepth) {
        case terminal::ColourDepth::None:
            log("Terminal colour depth = none (mono)");
            break;
        case terminal::ColourDepth::Ansi16:
            log("Terminal colour depth = 16");
            break;
        case terminal::ColourDepth::Ansi256:
            log("Terminal colour depth = 256");
            break;
        case terminal::ColourDepth::TrueColour:
            log("Terminal colour depth = TrueColour");
            break;
        default:
            assert("Unhandled colour depth");
    }
    if (actualColourDepth != colourDepth) {
        switch (actualColourDepth) {
            case terminal::ColourDepth::None:
                log("(Actual colour depth = none)");
                break;
            case terminal::ColourDepth::Ansi16:
                log("(Actual colour depth = 16)");
                break;
            case terminal::ColourDepth::Ansi256:
                log("(Actual colour depth = 256)");
                break;
            case terminal::ColourDepth::TrueColour:
                log("(Actual colour depth = TrueColour)");
                break;
            default:
                assert("Unhandled colour depth");
        }
    }
    m_term->setShutdownCheckFunction(
        []() -> bool { return mgo::shutdown_requested.load(std::memory_order_relaxed); });
    {
        terminal::ColourGuard _(m_term.get());
        m_term->setFgColour({ 106, 113, 247 });
        m_term->printAt(1, 2, "Loading data...");
#ifndef NDEBUG
        m_term->printAt(3, 2, "*** DEBUG BUILD *** (will be slower)");
#endif
    }
    m_term->cursorOff();
    m_term->render();
    int wordComplexity = static_cast<int>(m_cfg->readDouble("wordComplexityLevel", 3.0));
    log("Loading data... (word complexity {})", wordComplexity);
    m_ws = std::make_unique<wordSearcher::WordSearcher>(
        dataDir / std::format("words_{}.txt", wordComplexity),
        dataDir / "thesaurus.txt",
        dataDir / "definitions.txt",
        dataDir / "phrases.txt");
    log("Finished loading data");

    // Set up the menu and hotkeys.
    // Interleaving here to keep menu setup and key definitions next to each other.
    // Note hotkeys can be of type "Menu" or "Global".
    // "Global" hotkeys will be used even if in an input.
    // "Menu" hotkeys are only actioned when waiting for a command.
    m_menu->addItem(static_cast<int>(MenuItem::Jumble), "_Jumble");
    m_hotkeys.add('j', hotkeys::Type::Menu, CommandType::Jumble);
    m_hotkeys.add('J', hotkeys::Type::Global, CommandType::Jumble);
    m_menu->addItem(static_cast<int>(MenuItem::Reverse), "re_Verse");
    m_hotkeys.add('v', hotkeys::Type::Menu, CommandType::Reverse);
    m_hotkeys.add('V', hotkeys::Type::Global, CommandType::Reverse);
    m_menu->addItem(static_cast<int>(MenuItem::Regular), "re_Gular");
    m_hotkeys.add('g', hotkeys::Type::Menu, CommandType::Regular);
    m_hotkeys.add('G', hotkeys::Type::Global, CommandType::Regular);
    m_menu->addItem(static_cast<int>(MenuItem::Thesaurus), "_Thesaurus");
    m_hotkeys.add('t', hotkeys::Type::Menu, CommandType::Thesaurus);
    m_hotkeys.add('T', hotkeys::Type::Global, CommandType::Thesaurus);
    m_menu->addItem(static_cast<int>(MenuItem::Lookup), "_Lookup");
    m_hotkeys.add('l', hotkeys::Type::Menu, CommandType::Lookup);
    m_menu->addItem(static_cast<int>(MenuItem::Define), "_Define");
    m_hotkeys.add('d', hotkeys::Type::Menu, CommandType::Define);
    m_menu->addNewLine();
    m_menu->addItem(static_cast<int>(MenuItem::Filter), "f_Ilter");
    m_hotkeys.add('i', hotkeys::Type::Menu, CommandType::Filter);
    m_menu->addItem(static_cast<int>(MenuItem::Done), "_^_Done");
    m_hotkeys.add('D', hotkeys::Type::Global, CommandType::Done);
    m_hotkeys.add(keyPress::CTRL_D, hotkeys::Type::Global, CommandType::Done);
    m_menu->addItem(static_cast<int>(MenuItem::Save), "_^_Save");
    m_hotkeys.add('S', hotkeys::Type::Global, CommandType::Save);
    m_hotkeys.add(keyPress::CTRL_S, hotkeys::Type::Global, CommandType::Save);
    m_menu->addItem(static_cast<int>(MenuItem::Load), "_^_Load");
    m_hotkeys.add('L', hotkeys::Type::Global, CommandType::Load);
    m_hotkeys.add(keyPress::CTRL_L, hotkeys::Type::Global, CommandType::Load);
    m_menu->addItem(static_cast<int>(MenuItem::Restart), "_^_Restart");
    m_hotkeys.add(keyPress::CTRL_R, hotkeys::Type::Global, CommandType::Restart);
    m_hotkeys.add('R', hotkeys::Type::Global, CommandType::HardRestart); // Note HardRestart
    m_menu->addItem(static_cast<int>(MenuItem::Quit), "_^_Quit");
    m_hotkeys.add(keyPress::CTRL_Q, hotkeys::Type::Global, CommandType::Quit);
    m_hotkeys.add('Q', hotkeys::Type::Global, CommandType::Quit);

    // Other hotkeys not menu specific:
    m_hotkeys.add('s', hotkeys::Type::Menu, CommandType::EnterSearchString);
    m_hotkeys.add('f', hotkeys::Type::Menu, CommandType::EnterFoundString);
    m_hotkeys.add('c', hotkeys::Type::Menu, CommandType::EnterComment);
    m_hotkeys.add('n', hotkeys::Type::Menu, CommandType::EnterClueNumber);
    m_hotkeys.add(':', hotkeys::Type::Global, CommandType::AwaitCommand);
    m_hotkeys.add(keyPress::ESC, hotkeys::Type::Global, CommandType::AwaitCommand);
    m_hotkeys.add(keyPress::DOWN, hotkeys::Type::Menu, CommandType::ResultsScrollDown);
    m_hotkeys.add(keyPress::UP, hotkeys::Type::Menu, CommandType::ResultsScrollUp);
    m_hotkeys.add(keyPress::SPACE, hotkeys::Type::Menu, CommandType::ResultsPageDown);
    m_hotkeys.add(keyPress::PGDN, hotkeys::Type::Menu, CommandType::ResultsPageDown);
    m_hotkeys.add(keyPress::CTRL_F, hotkeys::Type::Menu, CommandType::ResultsPageDown);
    m_hotkeys.add(keyPress::PGUP, hotkeys::Type::Menu, CommandType::ResultsPageUp);
    m_hotkeys.add(keyPress::CTRL_B, hotkeys::Type::Menu, CommandType::ResultsPageUp);
    m_hotkeys.add(keyPress::F12, hotkeys::Type::Menu, CommandType::ShowDebugLog);
    m_hotkeys.add(keyPress::FOCUS_IN, hotkeys::Type::Global, CommandType::GainedFocus);
    m_hotkeys.add(keyPress::FOCUS_OUT, hotkeys::Type::Global, CommandType::LostFocus);
}

void Ui::checkForTerminalResize()
{
    if (!checkTerminalLargeEnough()) {
        throw(std::runtime_error("Terminal size is too small!"));
    }
    auto [rows, cols] = m_term->getTerminalSize();
    if (m_termSize.rows != rows || m_termSize.cols != cols) {
        log("Terminal size is currently {} rows by {} cols", rows, cols);
        m_termSize.rows = rows;
        m_termSize.cols = cols;
    }
}

bool Ui::checkTerminalLargeEnough()
{
    auto [rows, cols] = m_term->getTerminalSize();
    // Fairly arbitrary minimum terminal size
    if (rows < m_menuRowSize + m_headerRowSize + 5 || cols < 20) {
        return false;
    }
    return true;
}

int Ui::run()
{
    bool finished { false };
    // Start with search string input:
    m_commandQueue.emplace_back(CommandType::EnterSearchString);
    while (!finished && !mgo::shutdown_requested.load(std::memory_order_relaxed)) {
        if (!m_suppressRedraw) {
            redraw();
        }
        int keyPress;
        Command cmd(CommandType::NoOp);
        while (cmd.commandType == CommandType::NoOp) {
            if (!m_commandQueue.empty()) {
                cmd = *m_commandQueue.begin();
                m_commandQueue.pop_front();
            } else {
                keyPress = m_term->getChar();
                cmd = decodeKeyPress(keyPress);
            }
            // If a command handles its own redraw for performance
            // (e.g. scrolling results) then it can set m_suppressRedraw to true
            m_suppressRedraw = false;
            switch (cmd.commandType) {
                case CommandType::NoOp:
                    break;
                case CommandType::AwaitCommand: // Used if user presses ':'
                    enterExtendedCommand();
                    break;
                case CommandType::Jumble:
                    jumble();
                    break;
                case CommandType::Reverse:
                    reverse();
                    break;
                case CommandType::Regular:
                    regular();
                    break;
                case CommandType::Thesaurus:
                    thesaurus();
                    break;
                case CommandType::Lookup:
                    lookup();
                    break;
                case CommandType::Define:
                    define();
                    break;
                case CommandType::Filter:
                    filterResults();
                    break;
                case CommandType::Done:
                    done();
                    break;
                case CommandType::Save:
                    save();
                    break;
                case CommandType::Load:
                    load(cmd.data);
                    break;
                case CommandType::Restart:
                    restart();
                    break;
                case CommandType::HardRestart:
                    restart(true);
                    break;
                case CommandType::Quit:
                    finished = true;
                    break;
                case CommandType::EnterSearchString:
                    enterSearchString();
                    break;
                case CommandType::EnterFoundString:
                    enterFoundString();
                    break;
                case CommandType::EnterComment:
                    enterCommentString();
                    break;
                case CommandType::EnterClueNumber:
                    enterClueNumber();
                    break;
                case CommandType::ResultsScrollDown:
                    m_suppressRedraw = true;
                    scrollDownResults();
                    break;
                case CommandType::ResultsScrollUp:
                    m_suppressRedraw = true;
                    scrollUpResults();
                    break;
                case CommandType::ResultsPageDown:
                    pageDownResults();
                    break;
                case CommandType::ResultsPageUp:
                    pageUpResults();
                    break;
                case CommandType::ResultsSelection:
                    if (m_results.selectedItem.has_value()) {
                        if (m_results.selectedItem.value() < m_results.vec.size()) {
                            log("Results item {} ('{}') selected",
                                m_results.selectedItem.value(),
                                m_results.vec.at(m_results.selectedItem.value()));
                        }
                    }
                    break;
                case CommandType::LostFocus:
                    lostFocus();
                    break;
                case CommandType::GainedFocus:
                    // Does nothing
                    break;
                case CommandType::ShowDebugLog:
                    ShowDebugLog();
                    break;
                case CommandType::ExpandDefinition:
                    expandDefinition(cmd.data);
                    break;
            }
        }
    }
    return 0;
}

void Ui::clearResults(terminal::OutputMode mode)
{
    m_results.vec.clear();
    m_results.scrollOffset = 0;
    m_results.filtered = false;
    m_results.type = ResultsType::FreeForm;
    if (mode == terminal::OutputMode::immediate) {
        // If it's immediate we want to clear the results pane:
        for (size_t r = m_resultsTopRow + 1; r < m_resultsTopRow + getResultsPaneRowSize(); ++r) {
            m_term->goTo(r, 0, terminal::OutputMode::immediate);
            m_term->clearLine(terminal::OutputMode::immediate);
        }
    }
}

void Ui::setResults(const std::vector<std::string>& vec, ResultsType type /* = FreeForm */)
{
    clearResults(terminal::OutputMode::immediate);
    m_results.vec = vec;
    m_results.type = type;
    m_results.scrollOffset = 0;
}

void Ui::setResults(std::string_view result, ResultsType type /* = FreeForm */)
{
    clearResults(terminal::OutputMode::immediate);
    m_results.vec.emplace_back(result);
    m_results.type = type;
    m_results.scrollOffset = 0;
}

void Ui::appendResults(std::string_view item, ResultsType type)
{
    m_results.vec.emplace_back(item);
    m_results.type = type;
}

void Ui::displayHeader(terminal::OutputMode mode)
{
    constexpr terminal::ColourRgb ENTRY_COLOUR = { 160, 255, 100 };
    constexpr terminal::ColourRgb BRIGHT = { 200, 255, 200 };
    // Can use immediate mode to clear the header before an input (which is immediate)
    m_term->goTo(1, 1, mode);
    m_term->printMenuString(terminal::Colour::Default, BRIGHT, "_Search : ", mode);
    if (!m_clue.searchString.empty()) {
        {
            terminal::ColourGuard _(m_term.get());
            m_term->setFgColour(ENTRY_COLOUR);
            m_term->printAt(1, 10, m_clue.searchString, mode);
        }
        m_term->styleItalic(true, mode);
        m_term->print(
            std::format("  ({})", letterCount(m_clue.searchString, m_clue.foundString)), mode);
        m_term->styleItalic(false, mode);
    }
    m_term->clearToEndOfLine(mode);
    m_term->goTo(2, 1, mode);
    m_term->printMenuString(terminal::Colour::Default, BRIGHT, "_Found  : ", mode);
    m_term->clearToEndOfLine(mode);
    {
        terminal::ColourGuard _(m_term.get());
        m_term->setFgColour(ENTRY_COLOUR);
        m_term->printAt(2, 10, m_clue.foundString, mode);
    }
    m_term->clearToEndOfLine(mode);
    m_term->goTo(3, 1, mode);
    m_term->printMenuString(terminal::Colour::Default, BRIGHT, "_Comment: ", mode);
    m_term->clearToEndOfLine(mode);
    {
        terminal::ColourGuard _(m_term.get());
        m_term->setFgColour(ENTRY_COLOUR);
        m_term->printAt(3, 10, m_clue.comment, mode);
    }
    m_term->clearToEndOfLine(mode);
    m_term->goTo(4, 1, mode);
    m_term->printMenuString(terminal::Colour::Default, BRIGHT, "Clue _No: ", mode);
    {
        terminal::ColourGuard _(m_term.get());
        m_term->setFgColour(ENTRY_COLOUR);
        m_term->printAt(4, 10, m_clue.clueNumber, mode);
    }
    m_term->clearToEndOfLine(mode);
}

void Ui::displayResults(terminal::OutputMode mode)
{
    std::size_t lastRowInSection = m_resultsTopRow + getResultsPaneRowSize() - 1;
    if (!m_suppressRedraw) {
        hr(m_resultsTopRow, mode);

        m_term->setFgColour(terminal::Colour::Default, terminal::OutputMode::immediate);
        m_term->printAt(m_resultsTopRow, 1, "Results", mode);
        if (m_results.filtered) {
            m_term->printAt(m_resultsTopRow, 9, "(filtered)", mode);
        }
    }

    if (!m_results.vec.empty()) {
        terminal::ColourGuard _(m_term.get());
        m_term->setFgColour(terminal::Colour::BrightYellow, mode);
        std::size_t currentRow = m_resultsTopRow + 2;
        if (m_results.scrollOffset != 0) {
            m_term->printAt(currentRow - 1, 1, "...", mode);
        } else {
            m_term->printAt(currentRow - 1, 1, "   ", mode);
        }
        for (std::size_t p = m_results.scrollOffset; p < m_results.vec.size(); ++p) {
            if (m_results.vec[p].size() > m_termSize.cols - 2) {
                m_term->printAt(
                    currentRow, 1, m_results.vec[p].substr(0, m_termSize.cols - 5) + "...", mode);
            } else {
                m_term->printAt(currentRow, 1, m_results.vec[p], mode);
            }
            m_term->clearToEndOfLine(mode);
            ++currentRow;
            if (currentRow == lastRowInSection) {
                if (p < m_results.vec.size() - 1) {
                    // It wasn't the last row in m_results
                    m_term->printAt(currentRow, 1, "...", mode);
                    m_results.scrollAtBottom = false;
                } else {
                    m_term->printAt(currentRow, 1, "   ", mode);
                    m_results.scrollAtBottom = true;
                }
                break;
            }
            // if we didn't break then we must be at the bottom
            m_results.scrollAtBottom = true;
        }
    }
}

void Ui::displayMenu(terminal::OutputMode mode)
{
    const std::size_t topRow = m_termSize.rows - m_menuRowSize;
    hr(topRow, mode);
    m_term->printAt(topRow, 1, "Menu", mode);
    m_menu->printMenu(topRow + 1, 1, mode);
    m_term->cursorOff(mode);
}

void Ui::redraw()
{
    checkForTerminalResize();
    displayHeader();
    displayResults();
    displayMenu();
    m_term->render();
}

void Ui::restart(bool force)
{
    if (m_clue.dirty && !force) {
        terminal::MessageBoxOptions opts;
        opts.row = m_resultsTopRow + 2;
        opts.col = 2;
        opts.message = "Clue has changed!\nContinue?";
        opts.type = terminal::MessageBoxType::YesNo;
        int key = messageBox(opts);
        if (key != 'y' && key != 'Y') {
            return;
        }
    }
    clearResults(terminal::OutputMode::immediate);
    m_clue.searchString.clear();
    m_clue.foundString.clear();
    m_clue.clueNumber.clear();
    m_clue.comment.clear();
    m_clue.dirty = false;
    clearResults();
    m_commandQueue.emplace_back(CommandType::EnterSearchString);
}

void Ui::hr(std::size_t row, terminal::OutputMode mode)
{
    m_term->goTo(row, 0, mode);
    std::string hr;
    for (std::size_t c = 0; c < m_termSize.cols; ++c) {
        if (m_term->utf8Supported()) {
            hr.append("─"); // UTF-8 line character
        } else {
            hr.append("-"); // plain
        }
    }
    terminal::ColourGuard guard(m_term.get());
    m_term->setFgColour({ 0, 96, 0 }, mode);
    m_term->print(hr, mode);
}

void Ui::jumble(std::string foundString, terminal::OutputMode mode)
{
    if (foundString.empty()) {
        foundString = m_clue.foundString;
    }

    std::string foundLetters;

    // What are our found letters? Exclude '.' and '/'
    std::ranges::copy_if(
        foundString, std::back_inserter(foundLetters), [](char c) { return c != '.' && c != '/'; });
    // Which letters are remaining in the search string?
    std::unordered_map<char, int> freq;
    for (char c : foundLetters) {
        ++freq[c];
    }
    std::string remainingLetters;
    std::ranges::copy_if(
        m_clue.searchString, std::back_inserter(remainingLetters), [&freq](char c) {
            auto it = freq.find(c);
            if (it != freq.end() && it->second > 0) {
                --it->second;
                return false;
            }
            return true;
        });
    // Shuffle remainingLetters:
    std::random_device rd;
    std::mt19937 gen { rd() };
    std::ranges::shuffle(remainingLetters, gen);
    auto grid = lettersInACircle(remainingLetters);
    clearResults(mode);
    m_results.vec.emplace_back("");
    for (const auto& s : grid) {
        m_results.vec.emplace_back(" " + s);
    }
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back("");

    std::string alreadyFound;
    for (const auto& c : foundString) {
        if (c == '.') {
            alreadyFound.append("_ ");
        } else {
            alreadyFound.append(std::format("{} ", c));
        }
    }
    m_results.vec.emplace_back(alreadyFound);
    m_results.type = ResultsType::Jumble;
    if (mode == terminal::OutputMode::immediate) {
        displayResults(mode);
    }
}

void Ui::lookup()
{
    if (m_clue.searchString.empty() && m_clue.foundString.empty()) {
        return;
    } else {
        terminal::MessageBoxOptions opts;
        opts.row = 8;
        opts.col = 3;
        opts.type = terminal::MessageBoxType::YesNo;
        opts.message = "Cheat warning!\nAre you sure?";
        int key = messageBox(opts);
        if (key != 'y' && key != 'Y') {
            return;
        }
        redraw();
    }
    terminal::MessageBoxOptions opts;
    opts.row = 8;
    opts.col = 3;
    opts.message = "Searching...";
    messageBox(opts);
    clearResults();
    std::string lowerCase { m_clue.foundString };
    std::transform(
        m_clue.foundString.begin(),
        m_clue.foundString.end(),
        lowerCase.begin(),
        [](unsigned char c) { return ascii::tolower(c); });
    std::ranges::replace(lowerCase, '/', ' ');
    // Don't allow dots to match on spaces:
    lowerCase = std::regex_replace(lowerCase, std::regex("\\."), "[a-z]");
    auto results = m_ws->regexSearch(lowerCase);
    if (!m_clue.searchString.empty()) {
        std::string sortedSearchString { m_clue.searchString };
        std::transform(
            sortedSearchString.begin(),
            sortedSearchString.end(),
            sortedSearchString.begin(),
            [](unsigned char c) { return ascii::tolower(c); });
        std::ranges::sort(sortedSearchString);
        for (const auto& word : results) {
            // Ensure that any regex match actually contains the letters
            // in m_clue.searchString
            std::string w { word };
            w.erase(std::remove(w.begin(), w.end(), ' '), w.end());
            std::string sortedWord { w };
            std::ranges::sort(sortedWord);
            if (sortedWord == sortedSearchString) {
                m_results.vec.emplace_back(word);
            }
        }
    } else {
        // if search string is empty we add all results
        for (const auto& word : results) {
            m_results.vec.emplace_back(word);
        }
    }
    if (m_results.vec.empty()) {
        setResults("-- no matches found --");
    } else {
        m_results.type = ResultsType::Words;
    }
}

void Ui::regular()
{
    clearResults();
    if (m_clue.searchString.empty()) {
        setResults("Please enter a search string first");
        return;
    }
    m_results.vec.emplace_back("Every two letters");
    m_results.vec.emplace_back("");
    std::string t = utils::everyNth(m_clue.searchString, 2);
    m_results.vec.emplace_back(std::format("  Odd:   {} ({} letters)", t, t.size()));
    t = utils::everyNth({ m_clue.searchString.begin() + 1, m_clue.searchString.end() }, 2);
    m_results.vec.emplace_back(std::format("  Even:  {} ({} letters)", t, t.size()));
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back("Every three letters");
    m_results.vec.emplace_back("");
    t = utils::everyNth(m_clue.searchString, 3);
    m_results.vec.emplace_back(std::format("  Odd:   {} ({} letters)", t, t.size()));
    t = utils::everyNth({ m_clue.searchString.begin() + 1, m_clue.searchString.end() }, 3);
    m_results.vec.emplace_back(std::format("  Even:  {} ({} letters)", t, t.size()));
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back("Every two letters (reversed)");
    m_results.vec.emplace_back("");
    std::string reverseSearchString { m_clue.searchString };
    std::reverse(reverseSearchString.begin(), reverseSearchString.end());
    t = utils::everyNth(reverseSearchString, 2);
    m_results.vec.emplace_back(std::format("  Odd:   {} ({} letters)", t, t.size()));
    t = utils::everyNth({ reverseSearchString.begin() + 1, reverseSearchString.end() }, 2);
    m_results.vec.emplace_back(std::format("  Even:  {} ({} letters)", t, t.size()));
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back("Every three letters (reversed)");
    m_results.vec.emplace_back("");
    t = utils::everyNth(reverseSearchString, 3);
    m_results.vec.emplace_back(std::format("  Odd:   {} ({} letters)", t, t.size()));
    t = utils::everyNth({ reverseSearchString.begin() + 1, reverseSearchString.end() }, 3);
    m_results.vec.emplace_back(std::format("  Even:  {} ({} letters)", t, t.size()));
}

void Ui::reverse()
{
    clearResults();
    if (m_clue.searchString.empty()) {
        setResults("Please enter a search string first");
        return;
    }
    std::string reversed { m_clue.searchString };
    std::reverse(reversed.begin(), reversed.end());
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back(std::format("'{}' reversed is:", m_clue.searchString));
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back(reversed);
}

void Ui::thesaurus()
{
    clearResults();
    std::string lowercaseSearchString { m_clue.searchString };
    if (m_clue.searchString.empty()) {
        setResults("Please enter a search string first");
        return;
    }
    std::transform(
        lowercaseSearchString.begin(),
        lowercaseSearchString.end(),
        lowercaseSearchString.begin(),
        ascii::tolower);
    setResults(m_ws->thesaurus(lowercaseSearchString));
    if (m_results.vec.empty()) {
        setResults(std::format("--- {} not found ---", m_clue.searchString));
        return;
    }
    m_results.type = ResultsType::Words;
}

void Ui::define()
{
    if (m_results.type == ResultsType::Words) {
        m_results.vec = m_ws->definitions(m_results.vec);
        m_results.type = ResultsType::Definitions;
    }
}

void Ui::expandDefinition(std::string_view word)
{
    clearResults();
    m_results.type = ResultsType::FreeForm;
    auto vec = m_ws->definitions(word);
    if (vec.size() > 1) {
        m_results.vec.push_back(std::format("Definitions for '{}':", word));
    } else {
        m_results.vec.push_back(std::format("Definition for '{}':", word));
    }
    m_results.vec.push_back("");
    for (const auto& def : vec) {
        m_results.vec.push_back(def);
    }
}

void Ui::done()
{
    if (!m_clue.clueNumber.empty()) {
        m_savedClues.erase(m_clue.clueNumber);
    }
    restart(true);
}

void Ui::load(std::string resultsLine)
{
    if (m_savedClues.empty()) {
        setResults("No saved clues.");
        return;
    }
    std::string clueNumber;
    if (resultsLine.empty()) {
        clearResults(terminal::OutputMode::immediate);
        std::vector<std::string> vec;
        for (const auto& [clueNo, clue] : m_savedClues) {
            std::string entry = std::format("{:>4} : ", clueNo);
            entry.append(std::format("'{}', '{}'", clue.searchString, clue.foundString));
            if (!clue.comment.empty()) {
                entry.append(std::format(", {}", clue.comment));
            }
            vec.emplace_back(entry);
        }
        std::sort(vec.begin(), vec.end());
        for (const auto& s : vec) {
            appendResults(s);
        }
        m_results.type = ResultsType::Load;
        displayResults(terminal::OutputMode::immediate);
        terminal::InputOptions opts;
        opts.row = 4;
        opts.col = 10;
        opts.bgColour = terminal::Colour::Grey;
        opts.fgColour = terminal::Colour::BrightWhite;
        opts.mode = terminal::Mode::Insert;
        // Capital alphas & numbers only:
        opts.keysAllowed = terminal::keysAllowed::alpha | terminal::keysAllowed::numeric
            | terminal::keysAllowed::upper;
        opts.maxLen = 4;
        clueNumber = input(opts);
    } else {
        clueNumber = resultsLine.substr(0, 4);
        utils::trim(clueNumber);
    }
    if (!clueNumber.empty()) {
        auto it = m_savedClues.find(clueNumber);
        if (it == m_savedClues.end()) {
            setResults(std::format("Cannot find clue '{}'", clueNumber));
            return;
        }
        restart();
        m_clue.searchString = it->second.searchString;
        m_clue.foundString = it->second.foundString;
        m_clue.clueNumber = it->second.clueNumber;
        m_clue.comment = it->second.comment;
    }
    clearResults(terminal::OutputMode::immediate);
}

void Ui::save()
{
    terminal::MessageBoxOptions opts;
    opts.col = 2;
    opts.row = m_resultsTopRow + 2;
    opts.mode = terminal::OutputMode::render;
    if (m_clue.clueNumber.empty()) {
        opts.message = "Please enter a clue number first";
        messageBox(opts);
        return;
    }
    m_savedClues[m_clue.clueNumber] = m_clue;
    opts.message = std::format("Clue saved as '{}'.", m_clue.clueNumber);
    messageBox(opts);
    m_clue.dirty = false;
}

void Ui::filterResults()
{
    if (m_results.vec.empty() || m_results.type != ResultsType::Words) {
        return;
    }
    terminal::MessageBoxOptions msgboxOpts;
    msgboxOpts.row = m_resultsTopRow + 2;
    msgboxOpts.col = 2;
    msgboxOpts.message = "Enter filter string.\nWill drop non-matches.";
    messageBox(msgboxOpts);
    terminal::InputOptions inputOpts;
    inputOpts.row = m_termSize.rows - 1;
    inputOpts.col = 1;
    inputOpts.overrideCursorType = terminal::CursorType::BlockBlinking;
    std::string filter = m_term->input(inputOpts).enteredString;
    std::transform(filter.begin(), filter.end(), filter.begin(), ascii::tolower);
    // dots shouldn't match spaces
    filter = std::regex_replace(filter, std::regex("\\."), "[a-z]");
    // separators -> spaces
    filter = std::regex_replace(filter, std::regex("/"), " ");
    std::vector<std::string> newResults;
    std::string regexPrefix;
    if (!filter.contains("^")) {
        regexPrefix = "^.*";
    }
    std::string regexSuffix;
    if (!filter.contains("$")) {
        regexSuffix = ".*$";
    }
    filter = std::format("{}{}{}", regexPrefix, filter, regexSuffix);
    log("Filter regex: '{}'", filter);
    const std::regex regex(filter);
    for (const auto& w : m_results.vec) {
        if (std::regex_match(w, regex)) {
            newResults.emplace_back(w);
        }
    }
    setResults(newResults, ResultsType::Words);
    m_results.filtered = true;
}

void Ui::scrollDownResults()
{
    if (!m_results.scrollAtBottom) {
        ++m_results.scrollOffset;
    }
    displayResults(terminal::OutputMode::immediate);
}

void Ui::scrollUpResults()
{
    if (m_results.scrollOffset > 0) {
        --m_results.scrollOffset;
    }
    displayResults(terminal::OutputMode::immediate);
}

void Ui::pageDownResults()
{
    std::size_t resultsDisplaySize = getResultsPaneRowSize() - 3;
    if (m_results.scrollOffset + resultsDisplaySize < m_results.vec.size()) {
        m_results.scrollOffset += resultsDisplaySize;
    }
}

void Ui::pageUpResults()
{
    std::size_t resultsDisplaySize = getResultsPaneRowSize() - 3;
    if (m_results.scrollOffset >= resultsDisplaySize) {
        m_results.scrollOffset -= resultsDisplaySize;
    } else {
        m_results.scrollOffset = 0;
    }
}

void Ui::log(std::string_view logEntry [[maybe_unused]])
{
#ifndef NDEBUG
    m_debugLog.push_back(std::format("{}: {}", utils::currentTimeString(), logEntry));
    // Also write to file log
    mgo::Log::debug(logEntry);
#else
    if (m_debugLog.empty()) {
        m_debugLog.push_back("Debug log disabled in release build");
    }
#endif
}

std::filesystem::path Ui::locateDataDirectory(std::string_view argv0)
{
    const std::filesystem::path bin = std::filesystem::canonical(argv0);
    log("argv[0] = {}", bin.string());
    std::filesystem::path cwd = bin.parent_path();
    for (int n = 0; n < 3; ++n) {
        log("Searching for data files in {}", cwd.string());
        if (std::filesystem::exists(cwd / "words_1.txt")) {
            log("Data files found in {}", cwd.string());
            return cwd;
        }
        cwd = cwd.parent_path();
    }
    // If we get here we could not locate the data needed
    throw std::runtime_error("Could not locate data directory");
}

void Ui::enterFoundString()
{
    if (m_clue.searchString.empty()) {
        enterFoundStringUnconstrained();
    } else {
        enterFoundStringConstrained();
    }
}

void Ui::enterFoundStringConstrained()
{
    terminal::InputOptions opts;
    opts.mode = terminal::Mode::Overwrite;
    opts.defaultValue = m_clue.foundString;
    opts.row = 2;
    opts.col = 10;
    opts.bgColour = terminal::Colour::Grey;
    opts.fgColour = terminal::Colour::BrightWhite;
    opts.maxLen = separatedStringSize(opts.defaultValue);
    // Because we have a special use case here we allow all keys
    // and handle them specifically in the callback hook
    opts.reportStatus = terminal::InputReportStatus::Status;
    opts.preInsertHook = [&](int key) -> int {
        int rc = key;
        std::string lettersRemaining = m_clue.searchString;
        for (int n = 0; n == 0; ++n) {
            // For report status:
            for (const auto& c : opts.currentValue) {
                auto it = lettersRemaining.find(c);
                if (it != std::string::npos) {
                    lettersRemaining.erase(it, 1);
                }
            }

            if (key == keyPress::SPACE) {
                rc = keyPress::RIGHT;
                break;
            }
            if (key == keyPress::CTRL_U) {
                opts.currentValue = std::string(m_clue.searchString.size(), '.');
                opts.maxLen = opts.currentValue.size();
                opts.cursorPos = 0;
                lettersRemaining = m_clue.searchString;
                rc = keyPress::NO_KEY;
                break;
            }
            if (key < 0) {
                // disallow extended characters, e.g. 'é'
                rc = keyPress::NO_KEY;
                break;
            }
            if (key == keyPress::BACKSPACE && opts.cursorPos > 0
                && opts.currentValue.at(opts.cursorPos - 1) == '/') {
                --opts.maxLen;
                rc = key;
                break;
            }
            if (key == keyPress::BACKSPACE) {
                if (opts.cursorPos > 0 && opts.cursorPos < opts.currentValue.size() - 1) {
                    --opts.cursorPos;
                } else if (
                    opts.cursorPos == opts.currentValue.size() - 1
                    && opts.currentValue[opts.cursorPos] == '.' && opts.cursorPos > 0) {
                    --opts.cursorPos;
                }
                if (ascii::isalpha(opts.currentValue.at(opts.cursorPos))) {
                    lettersRemaining.push_back(opts.currentValue[opts.cursorPos]);
                }
                opts.currentValue[opts.cursorPos] = '.';
                rc = keyPress::NO_KEY;
                break;
            }
            if (key == keyPress::DELETE && opts.currentValue[opts.cursorPos] == '/') {
                opts.currentValue.erase(opts.cursorPos, 1);
                --opts.maxLen;
                rc = keyPress::NO_KEY;
                break;
            }
            if (key == keyPress::DELETE) {
                if (ascii::isalpha(opts.currentValue.at(opts.cursorPos))) {
                    lettersRemaining.push_back(opts.currentValue[opts.cursorPos]);
                }
                opts.currentValue[opts.cursorPos] = '.';
                rc = keyPress::NO_KEY;
                break;
            }
            if (key == '/' || ascii::isdigit(key)) {
                if (key != '/') { // i.e. is a digit
                    // move cursor the number entered
                    if (opts.cursorPos + (key - 48) > opts.currentValue.size() - 1) {
                        rc = keyPress::NO_KEY;
                        break;
                    }
                    opts.cursorPos += key - 48;
                    key = '/';
                }
                // Disallow entry of separator at beginning or end
                if (opts.cursorPos == 0 || opts.cursorPos > opts.currentValue.size() - 1) {
                    rc = keyPress::NO_KEY;
                    break;
                }
                // Disallow entry of separator immediately next to an existing separator
                if (opts.currentValue.at(opts.cursorPos) == '/'
                    || (opts.cursorPos > 0 && opts.currentValue.at(opts.cursorPos - 1) == '/')) {
                    rc = keyPress::NO_KEY;
                    break;
                }
                ++opts.maxLen;
                opts.currentValue.push_back(' ');
                // Shift any existing characters up by one (drops last character)
                for (std::size_t n = opts.maxLen - 1; n > opts.cursorPos; --n) {
                    opts.currentValue[n] = opts.currentValue[n - 1];
                }
                rc = key;
                break;
            }
            if (ascii::isprint(key)) {
                // Disallow any character not in search string IF the search string has been set
                if (!m_clue.searchString.empty()) {
                    if (ascii::toupper(key) == opts.currentValue.at(opts.cursorPos)) {
                        // we're just overwriting an existing "found" character
                        rc = ascii::toupper(key);
                        break;
                    }
                    auto c1 = std::count(
                        m_clue.searchString.begin(),
                        m_clue.searchString.end(),
                        ascii::toupper(key));
                    auto c2 = std::count(
                        opts.currentValue.begin(), opts.currentValue.end(), ascii::toupper(key));
                    if (key != '.' && (c1 == 0 || c2 == c1)) {
                        m_term->bell(terminal::OutputMode::immediate);
                        rc = keyPress::NO_KEY;
                        break;
                    }
                    if (key != '.' && !ascii::isalpha(key)) {
                        m_term->bell(terminal::OutputMode::immediate);
                        rc = keyPress::NO_KEY;
                        break;
                    } else {
                        if (ascii::isalpha(opts.currentValue[opts.cursorPos])) {
                            lettersRemaining.push_back(opts.currentValue[opts.cursorPos]);
                        }
                        int k = ascii::toupper(key);
                        auto it = lettersRemaining.find(k);
                        if (it != std::string::npos) {
                            lettersRemaining.erase(it, 1);
                        }
                        rc = k;
                        break;
                    }
                } else {
                    rc = ascii::toupper(key);
                    break;
                }
            }
        }
        if (lettersRemaining.empty()) {
            opts.statusData = "";
        } else {
            opts.statusData = "Letters remaining: " + lettersRemaining + " ";
        }
        return rc;
    };
    opts.postInsertHook = [&]() -> bool {
        // Always pad out with dots if smaller than default size
        std::size_t cvSize = separatedStringSize(opts.currentValue); // ignores word separators
        std::size_t dfSize = separatedStringSize(opts.defaultValue); // ditto
        if (cvSize < dfSize) {
            opts.currentValue.append(std::string(dfSize - cvSize, '.'));
        }
        return true;
    };
    opts.afterEveryIterationHook = [&]() {
        if (m_results.type == ResultsType::Jumble && opts.currentValue != opts.previousValue) {
            jumble(opts.currentValue, terminal::OutputMode::immediate);
        }
    };
    if (!m_clue.foundString.empty()) {
        // May have been extended with word separators
        opts.maxLen = m_clue.foundString.size();
    } else {
        opts.maxLen = m_clue.searchString.size();
    }
    m_clue.foundString = input(opts);
    m_clue.dirty = true;
    log("m_clue.foundString (constrained) input: '{}'", m_clue.foundString);
    if (opts.entryKey == keyPress::TAB || opts.entryKey == keyPress::DOWN) {
        // chain to comment entry
        m_commandQueue.emplace_back(CommandType::EnterComment);
    }
    if (opts.entryKey == keyPress::SHIFT_TAB || opts.entryKey == keyPress::UP) {
        // chain to search entry
        m_commandQueue.emplace_back(CommandType::EnterSearchString);
    }
}

void Ui::enterFoundStringUnconstrained()
{
    displayHeader(terminal::OutputMode::immediate);
    terminal::InputOptions opts;
    opts.defaultValue = m_clue.foundString;
    opts.row = 2;
    opts.col = 10;
    opts.bgColour = terminal::Colour::Grey;
    opts.fgColour = terminal::Colour::BrightWhite;
    opts.preInsertHook = [&](int key) -> int {
        if (key == ' ') {
            return '.';
        }
        if (key == '/' || key == '.' || ascii::isalpha(key)) {
            return ascii::toupper(key);
        }
        if (ascii::isdigit(key)) {
            opts.currentValue.insert(opts.cursorPos, std::string(key - 48, '.'));
            opts.cursorPos += key - 48;
            return keyPress::NO_KEY;
        }
        // Because we've specified KeysAllowed::All we need to let these special keys
        // through but not others.
        // TODO: Could use the new bitmask to simplify this
        if (key == keyPress::BACKSPACE || key == keyPress::LEFT || key == keyPress::RIGHT
            || key == keyPress::DELETE || key == keyPress::CTRL_A || key == keyPress::CTRL_E
            || key == keyPress::END || key == keyPress::HOME || key == keyPress::CTRL_U
            || key == keyPress::ENTER || key == keyPress::ESC || key == keyPress::TAB
            || key == keyPress::SHIFT_TAB || key == keyPress::UP || key == keyPress::DOWN) {
            return key;
        }
        return keyPress::NO_KEY;
    };
    while (true) {
        m_clue.foundString = input(opts);
        if (m_clue.foundString.starts_with('/') || m_clue.foundString.ends_with('/')) {
            opts.defaultValue = m_clue.foundString;
            setResults("Found string cannot start with or end with a separator ('/')");
            displayResults(terminal::OutputMode::immediate);
            opts.cursorPos = 0;
            continue;
        }
        if (m_clue.foundString.contains("//")) {
            opts.defaultValue = m_clue.foundString;
            setResults("Found string cannot contain two or more consecutive separators ('/')");
            displayResults(terminal::OutputMode::immediate);
            opts.cursorPos = 0;
            continue;
        }
        clearResults(terminal::OutputMode::immediate);
        break;
    }
    m_clue.dirty = true;
    log("m_clue.foundString (unconstrained) input: '{}'", m_clue.foundString);
    if (opts.entryKey == keyPress::TAB || opts.entryKey == keyPress::DOWN) {
        // chain to comment entry
        m_commandQueue.emplace_back(CommandType::EnterComment);
    }
    if (opts.entryKey == keyPress::SHIFT_TAB || opts.entryKey == keyPress::UP) {
        // chain to search entry
        m_commandQueue.emplace_back(CommandType::EnterSearchString);
    }
}

void Ui::enterSearchString()
{
    displayHeader(terminal::OutputMode::immediate);
    terminal::InputOptions opts;
    opts.defaultValue = m_clue.searchString;
    opts.row = 1;
    opts.col = 10;
    opts.bgColour = terminal::Colour::Grey;
    opts.fgColour = terminal::Colour::BrightWhite;
    opts.keysAllowed = terminal::keysAllowed::alpha | terminal::keysAllowed::upper;
    opts.reportStatus = terminal::InputReportStatus::SizeInLetters;
    opts.preInsertHook = [&](int key) -> int {
        // Disallow spaces
        if (key == ' ') {
            m_term->bell(terminal::OutputMode::immediate);
            return keyPress::NO_KEY;
        }
        if (key < keyPress::NO_KEY) {
            // disallow extended characters
            return keyPress::NO_KEY;
        }
        return key;
    };
    m_clue.searchString = input(opts);
    if (!m_clue.searchString.empty()
        && separatedStringSize(m_clue.foundString) != m_clue.searchString.size()) {
        m_clue.foundString = std::string(m_clue.searchString.size(), '.');
    }
    if (!m_clue.searchString.empty()) {
        m_clue.dirty = true;
        log("m_clue.searchString input: '{}'", m_clue.searchString);
    }
    if (opts.entryKey == keyPress::TAB || opts.entryKey == keyPress::DOWN) {
        // chain to enter found string
        m_commandQueue.emplace_back(CommandType::EnterFoundString);
    }
}

void Ui::enterCommentString()
{
    terminal::InputOptions opts;
    opts.row = 3;
    opts.col = 10;
    opts.bgColour = terminal::Colour::Grey;
    opts.fgColour = terminal::Colour::BrightWhite;
    opts.mode = terminal::Mode::Insert;
    opts.defaultValue = m_clue.comment;
    m_clue.comment = input(opts, false);
    m_clue.dirty = true;
    log("m_clue.comment input: '{}'", m_clue.comment);
    if (opts.entryKey == keyPress::TAB || opts.entryKey == keyPress::DOWN) {
        // chain to clue number entry
        m_commandQueue.emplace_back(CommandType::EnterClueNumber);
    }
    if (opts.entryKey == keyPress::SHIFT_TAB || opts.entryKey == keyPress::UP) {
        // chain to found entry
        m_commandQueue.emplace_back(CommandType::EnterFoundString);
    }
}

void Ui::enterClueNumber()
{
    terminal::InputOptions opts;
    opts.row = 4;
    opts.col = 10;
    opts.bgColour = terminal::Colour::Grey;
    opts.fgColour = terminal::Colour::BrightWhite;
    opts.mode = terminal::Mode::Insert;
    opts.defaultValue = m_clue.clueNumber;
    opts.keysAllowed = terminal::keysAllowed::alpha | terminal::keysAllowed::numeric
        | terminal::keysAllowed::upper;
    opts.maxLen = 4;
    m_clue.clueNumber = input(opts);
    if (!m_clue.clueNumber.empty()) {
        log("m_clue input: '{}'", m_clue.clueNumber);
        m_commandQueue.emplace_back(CommandType::Save);
    }
    if (opts.entryKey == keyPress::SHIFT_TAB || opts.entryKey == keyPress::UP) {
        // chain to comment entry
        m_commandQueue.emplace_back(CommandType::EnterComment);
    }
}

void Ui::enterExtendedCommand()
{
    terminal::InputOptions opts;
    opts.row = m_termSize.rows - 1;
    opts.col = 0;
    opts.prompt = ":";
    opts.bgColour = terminal::Colour::Default;
    opts.fgColour = terminal::Colour::BrightWhite;
    opts.mode = terminal::Mode::Overwrite;
    opts.overrideCursorType = terminal::CursorType::BlockBlinking;
    opts.keysAllowed = terminal::keysAllowed::alpha;
    std::string ec = input(opts, false); // false = don't allow hotkeys
    if (ec.empty()) {
        return;
    }
    switch (ec[0]) {
        case 'q':
        case 'Q':
            m_commandQueue.emplace_back(CommandType::Quit);
            break;
        case 's':
        case 'S':
        case 'w':
        case 'W':
            m_commandQueue.emplace_back(CommandType::Save);
            break;
        case 'e':
        case 'E':
        case 'l':
        case 'L':
            m_commandQueue.emplace_back(CommandType::Load);
            break;
        case 'r':
            m_commandQueue.emplace_back(CommandType::Restart);
            break;
        case 'R':
            m_commandQueue.emplace_back(CommandType::HardRestart);
            break;
        default:
            return;
    }
}

void Ui::ShowDebugLog()
{
    clearResults();
    setResults(m_debugLog);
}

void Ui::lostFocus()
{
    terminal::MessageBoxOptions opts;
    opts.message = "Focus lost";
    opts.alignRight = true;
    opts.col = m_termSize.cols - 2;
    opts.row = 1;
    opts.prompt = "Waiting...";
    opts.waitForKey = true;
    m_term->messageBox(opts);
    // The "focus gained" message will terminate (and
    // be swallowed by) the messageBox when focus is regained
}

int Ui::messageBox(terminal::MessageBoxOptions& opts)
{
    int key = m_term->messageBox(opts);
    if (key == keyPress::FOCUS_OUT) {
        m_commandQueue.emplace_back(CommandType::LostFocus);
        return keyPress::NO_KEY;
    } else if (key == keyPress::FOCUS_IN) {
        m_commandQueue.emplace_back(CommandType::GainedFocus);
        return keyPress::NO_KEY;
    } else {
        return key;
    }
}

std::size_t Ui::getResultsPaneRowSize()
{
    return m_termSize.rows - m_menuRowSize - m_headerRowSize;
}

Command Ui::decodeKeyPress(int keyPress)
{
    if (keyPress == keyPress::MOUSE) {
        return decodeMouseEvent(
            keyPress::lastMouseEvent.button,
            keyPress::lastMouseEvent.row,
            keyPress::lastMouseEvent.col);
    }
    auto cmd = m_hotkeys.getCommandFromKeyPress(keyPress);
    if (cmd.has_value()) {
        return Command(*cmd);
    }
    return Command(CommandType::NoOp);
}

Command Ui::decodeMouseEvent(int button, std::size_t row, std::size_t col)
{
    if (button != 64 && button != 65) { // don't log scroll events, too many
        log("Mouse event: Button: {}, Row: {}, Col: {}", button, row, col);
    }
    if (button == 0) {
        switch (row) {
            case 1:
                return Command(CommandType::EnterSearchString);
            case 2:
                return Command(CommandType::EnterFoundString);
            case 3:
                return Command(CommandType::EnterComment);
            case 4:
                return Command(CommandType::EnterClueNumber);
        }
        if (row > m_termSize.rows - m_menuRowSize) {
            // a click in the menu area
            std::optional<int> menuItem = m_menu->getIdFromHitBox(row, col);
            if (menuItem.has_value()) {
                switch (static_cast<MenuItem>(menuItem.value())) {
                    case MenuItem::Jumble:
                        return Command(CommandType::Jumble);
                    case MenuItem::Reverse:
                        return Command(CommandType::Reverse);
                    case MenuItem::Regular:
                        return Command(CommandType::Regular);
                    case MenuItem::Thesaurus:
                        return Command(CommandType::Thesaurus);
                    case MenuItem::Lookup:
                        return Command(CommandType::Lookup);
                    case MenuItem::Define:
                        return Command(CommandType::Define);
                    case MenuItem::Filter:
                        return Command(CommandType::Filter);
                    case MenuItem::Done:
                        return Command(CommandType::Done);
                    case MenuItem::Save:
                        return Command(CommandType::Save);
                    case MenuItem::Load:
                        return Command(CommandType::Load);
                    case MenuItem::Restart:
                        return Command(CommandType::Restart);
                    case MenuItem::Quit:
                        return Command(CommandType::Quit);
                }
            }
        }
    }
    if (row > m_headerRowSize && row < m_termSize.rows - m_menuRowSize) {
        // Scroll support for results pane
        // Currently only supports up (button 64) and down (button 65)
        // Horizontal scrolling would be button codes 96 (left) and 97 (right),
        // but support for this is patchy - it depends on both the terminal emulator
        // and the mouse/trackpad sending the events. Not needed yet, if at all.
        if (button == 65) { // scroll down
            if (m_results.scrollAtBottom) {
                return Command(CommandType::NoOp);
            }
            return Command(CommandType::ResultsScrollDown);
        }
        if (button == 64) { // scroll up
            if (m_results.scrollOffset == 0) {
                return Command(CommandType::NoOp);
            }
            return Command(CommandType::ResultsScrollUp);
        }
        if (button == 0
            && (m_results.type == ResultsType::Words || m_results.type == ResultsType::Load
                || m_results.type == ResultsType::Definitions)) {
            std::size_t selection = row - m_resultsTopRow - 2 + m_results.scrollOffset;
            if (m_results.vec.size() > selection) {
                m_results.selectedItem = selection;
                Command cmd(CommandType::ResultsSelection, m_results.vec.at(selection));
                if (m_results.type == ResultsType::Load) {
                    cmd.commandType = CommandType::Load;
                }
                if (m_results.type == ResultsType::Definitions) {
                    cmd.commandType = CommandType::ExpandDefinition;
                    auto v = utils::split(m_results.vec.at(selection), ':');
                    if (!v.empty()) {
                        cmd.data = v[0];
                    }
                }
                return cmd;
            }
        }
    }
    return Command(CommandType::NoOp);
}

std::string Ui::input(terminal::InputOptions& opts, bool useGlobalHotKeys /* = true */)
{
    redraw(); // to clear any message boxes
    if (useGlobalHotKeys) {
        opts.additionalEntryKeys = m_hotkeys.get(hotkeys::Type::Global);
    }
    terminal::InputResult inputResult = m_term->input(opts);
    if (opts.entryKey != keyPress::ENTER) {
        auto cmd = m_hotkeys.getCommandFromKeyPress(opts.entryKey);
        if (cmd.has_value()) {
            m_commandQueue.push_back(*cmd);
        }
    }
    if (inputResult.clickType == terminal::InputMouseClickType::ClickedOff) {
        m_commandQueue.push_back(
            decodeMouseEvent(0, inputResult.mouseClickRow, inputResult.mouseClickCol));
    }
    if (inputResult.lostFocus) {
        m_commandQueue.emplace_back(CommandType::LostFocus);
    }
    return inputResult.enteredString;
}

} // namespace ui
