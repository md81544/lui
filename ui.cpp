#include "ui.h"
#include "ascii.h"
#include "keypress.h"
#include "log.h"
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
#include <vector>

namespace {

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

} // anonymous namespace

namespace ui {

Ui::Ui(std::string_view argv0, int wordComplexity)
{
    if (!checkTerminalLargeEnough()) {
        throw(std::runtime_error("Terminal size is too small!"));
    }
    log("DEBUG LOG");
    const auto dataDir = locateDataDirectory(argv0);
    m_term.setShutdownCheckFunction(
        []() -> bool { return mgo::shutdown_requested.load(std::memory_order_relaxed); });
    m_term.printAt(1, 2, "Loading data...");
#ifndef NDEBUG
    m_term.printAt(3, 2, "*** DEBUG BUILD *** (will be slower)");
#endif
    m_term.cursorOff();
    m_term.render();
    log(std::format("Loading data... (word complexity {})", wordComplexity));
    m_ws = std::make_unique<wordSearcher::WordSearcher>(
        dataDir / std::format("words_{}.txt", wordComplexity),
        dataDir / "thesaurus.txt",
        dataDir / "definitions.txt",
        dataDir / "phrases.txt");
    log("Finished loading data");

    // Set up the menu
    m_menu.addItem(MENU_JUMBLE, "_Jumble");
    m_menu.addItem(MENU_REVERSE, "re_Verse");
    m_menu.addItem(MENU_REGULAR, "_Regular");
    m_menu.addItem(MENU_THESAURUS, "_Thesaurus");
    m_menu.addItem(MENU_LOOKUP, "_Lookup");
    m_menu.addItem(MENU_DEFINE, "_Define");
    m_menu.addNewLine();
    m_menu.addItem(MENU_FILTER, "f_Ilter");
    m_menu.addItem(MENU_SAVE, "_^_Save");
    m_menu.addItem(MENU_LOAD, "_^_Load");
    m_menu.addItem(MENU_RESTART, "_^_Restart");
    m_menu.addItem(MENU_QUIT, "_^_Quit");
}

void Ui::checkForTerminalResize()
{
    if (!checkTerminalLargeEnough()) {
        throw(std::runtime_error("Terminal size is too small!"));
    }
    auto [rows, cols] = m_term.getTerminalSize();
    if (m_termSize.rows != rows || m_termSize.cols != cols) {
        log(std::format("Terminal size is currently {} rows by {} cols", rows, cols));
        m_termSize.rows = rows;
        m_termSize.cols = cols;
    }
}

bool Ui::checkTerminalLargeEnough()
{
    auto [rows, cols] = m_term.getTerminalSize();
    // Fairly arbitrary minimum terminal size
    if (rows < m_menuRowSize + m_headerRowSize + 5 || cols < 20) {
        return false;
    }
    return true;
}

void Ui::clearCommandPrompt()
{
    // Clear command prompt immediately
    if (m_commandSeqCount > 0) {
        m_commandSeqCount = 0;
        displayMenu(terminal::OutputMode::immediate);
    }
}
int Ui::run()
{
    bool finished { false };
    while (!finished && !mgo::shutdown_requested.load(std::memory_order_relaxed)) {
        if (m_commandSeqCount > 0) {
            --m_commandSeqCount;
        }
        checkForTerminalResize();
        displayHeader();
        displayResults();
        displayMenu();
        m_term.render();
        int keyPress = m_term.getChar();
        switch (keyPress) {
            case keyPress::NO_KEY: // key was consumed by input handler
                break;
            case ':':
                m_commandSeqCount = 2;
                break;
            case keyPress::CTRL_R:
                // clear eveything down
                restart();
                break;
            case 'q':
            case 'Q':
                if (m_commandSeqCount > 0) {
                    // i.e. :q
                    finished = true;
                }
                break;
            case keyPress::CTRL_C:
            case keyPress::CTRL_Q:
                finished = true;
                break;
            case 'j':
            case 'J':
                jumble();
                break;
            case 't':
            case 'T':
                thesaurus();
                break;
            case 'l':
            case 'L':
                if (m_commandSeqCount > 0) {
                    // i.e. :l
                    load();
                } else {
                    lookup();
                }
                break;
            case 'i':
            case 'I':
                filterResults();
                break;
            case 'd':
            case 'D':
                define();
                break;
            case 'f':
            case 'F':
                enterFoundString();
                break;
            case 'c':
            case 'C':
                enterCommentString();
                break;
            case 'n':
            case 'N':
                enterClueNumber();
                break;
            case 's':
            case 'S':
                if (m_commandSeqCount > 0) {
                    // i.e. :s
                    save();
                } else {
                    enterSearchString();
                }
                break;
            case 'r':
            case 'R':
                if (m_commandSeqCount > 0) {
                    // i.e. :r
                    restart();
                    clearCommandPrompt(); // To wipe out the commmand prompt
                } else {
                    regular();
                }
                break;
            case 'v':
            case 'V':
                reverse();
                break;
            case keyPress::DOWN:
                if (!m_results.scrollAtBottom) {
                    ++m_results.scrollOffset;
                }
                break;
            case keyPress::UP:
                if (m_results.scrollOffset > 0) {
                    --m_results.scrollOffset;
                }
                break;
            case keyPress::PGDN:
            case keyPress::CTRL_F:
            case keyPress::SPACE:
                pageDownResults();
                break;
            case keyPress::PGUP:
            case keyPress::CTRL_B: // Note Ctrl-B may be TMux's "prefix" key!
                pageUpResults();
                break;
            case keyPress::F12:
                clearResults();
                setResults(m_debugLog);
                break;
            case keyPress::CTRL_S:
                save();
                break;
            case keyPress::CTRL_L:
                load();
                break;
            case keyPress::ESC:
                // currently does nothing
                break;
            case keyPress::MOUSE:
                {
                    log(std::format(
                        "Mouse event: Button: {}, Row: {}, Col: {}",
                        keyPress::lastMouseClick.button,
                        keyPress::lastMouseClick.row,
                        keyPress::lastMouseClick.col));
                    std::size_t clickRow = keyPress::lastMouseClick.row;
                    std::size_t clickCol = keyPress::lastMouseClick.col;
                    switch (clickRow) {
                        case 1:
                            enterSearchString();
                            break;
                        case 2:
                            enterFoundString();
                            break;
                        case 3:
                            enterCommentString();
                            break;
                        case 4:
                            enterClueNumber();
                            break;
                        default:
                            break;
                    }
                    if (clickRow > m_termSize.rows - m_menuRowSize) {
                        // a click in the menu area
                        std::optional<int> menuItem = m_menu.getIdFromHitBox(clickRow, clickCol);
                        if (menuItem.has_value()) {
                            switch (menuItem.value()) {
                                case MENU_JUMBLE:
                                    jumble();
                                    break;
                                case MENU_REVERSE:
                                    reverse();
                                    break;
                                case MENU_REGULAR:
                                    regular();
                                    break;
                                case MENU_THESAURUS:
                                    thesaurus();
                                    break;
                                case MENU_LOOKUP:
                                    lookup();
                                    break;
                                case MENU_DEFINE:
                                    define();
                                    break;
                                case MENU_FILTER:
                                    filterResults();
                                    break;
                                case MENU_SAVE:
                                    save();
                                    break;
                                case MENU_LOAD:
                                    load();
                                    break;
                                case MENU_RESTART:
                                    restart();
                                    break;
                                case MENU_QUIT:
                                    finished = true;
                                    break;
                                default:
                                    assert(false); // Item not handled; add item above
                            }
                        }
                    }
                    if (clickRow > m_headerRowSize && clickRow < m_termSize.rows - m_menuRowSize) {
                        // Scroll support for results pane
                        // Currently only supports up (button 64) and down (button 65)
                        // Horizontal scrolling would be button codes 96 (left) and 97 (right),
                        // but support for this is patchy - it depends on both the terminal emulator
                        // and the mouse/trackpad sending the events. Not needed yet, if at all.
                        if (keyPress::lastMouseClick.button == 65) { // scroll down
                            if (!m_results.scrollAtBottom) {
                                ++m_results.scrollOffset;
                            }
                        }
                        if (keyPress::lastMouseClick.button == 64) { // scroll up
                            if (m_results.scrollOffset > 0) {
                                --m_results.scrollOffset;
                            }
                        }
                    }
                }
                break;
            default:
                m_term.bell();
        }
    }
    return 0;
}

void Ui::clearResults(terminal::OutputMode mode)
{
    m_results.vec.clear();
    m_results.scrollOffset = 0;
    m_results.filtered = false;
    if (mode == terminal::OutputMode::immediate) {
        // If it's immediate we want to clear the results pane:
        for (size_t r = m_resultsTopRow + 1; r < m_resultsTopRow + getResultsPaneRowSize(); ++r) {
            m_term.goTo(r, 0, terminal::OutputMode::immediate);
            m_term.clearLine(terminal::OutputMode::immediate);
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
    // Can use immediate mode to clear the header before an input (which is immediate)
    m_term.goTo(1, 1, mode);
    m_term.printMenuString(
        terminal::Colour::Default, terminal::Colour::BrightWhite, "_Search : ", mode);
    if (!m_clue.searchString.empty()) {
        m_term.printAt(
            1,
            10,
            std::format("{}  ({} letters)", m_clue.searchString, m_clue.searchString.size()),
            mode);
    }
    m_term.clearToEndOfLine(mode);
    m_term.goTo(2, 1, mode);
    m_term.printMenuString(
        terminal::Colour::Default, terminal::Colour::BrightWhite, "_Found  : ", mode);
    m_term.clearToEndOfLine(mode);
    m_term.printAt(2, 10, m_clue.foundString, mode);
    m_term.clearToEndOfLine(mode);
    m_term.goTo(3, 1, mode);
    m_term.printMenuString(
        terminal::Colour::Default, terminal::Colour::BrightWhite, "_Comment: ", mode);
    m_term.clearToEndOfLine(mode);
    m_term.printAt(3, 10, m_clue.comment, mode);
    m_term.clearToEndOfLine(mode);
    m_term.goTo(4, 1, mode);
    m_term.printMenuString(
        terminal::Colour::Default, terminal::Colour::BrightWhite, "Clue _No: ", mode);
    m_term.printAt(4, 10, m_clue.clueNumber, mode);
    m_term.clearToEndOfLine(mode);
}

void Ui::displayResults(terminal::OutputMode mode)
{
    std::size_t lastRowInSection = m_resultsTopRow + getResultsPaneRowSize() - 1;
    hr(m_resultsTopRow, mode);

    m_term.printAt(m_resultsTopRow, 1, "Results", mode);
    if (m_results.filtered) {
        m_term.printAt(m_resultsTopRow, 9, "(filtered)", mode);
    }

    if (!m_results.vec.empty()) {
        terminal::Colour oldFgColour = m_term.getFgColour();
        m_term.setFgColour(terminal::Colour::BrightYellow, mode);
        std::size_t currentRow = m_resultsTopRow + 2;
        if (m_results.scrollOffset != 0) {
            m_term.printAt(currentRow - 1, 1, "...");
        }
        for (std::size_t p = m_results.scrollOffset; p < m_results.vec.size(); ++p) {
            if (m_results.vec[p].size() > m_termSize.cols - 2) {
                m_term.printAt(
                    currentRow, 1, m_results.vec[p].substr(0, m_termSize.cols - 5) + "...", mode);
            } else {
                m_term.printAt(currentRow, 1, m_results.vec[p], mode);
            }
            ++currentRow;
            if (currentRow == lastRowInSection) {
                if (p < m_results.vec.size() - 1) {
                    // It wasn't the last row in m_results
                    m_term.printAt(currentRow, 1, "...", mode);
                    m_results.scrollAtBottom = false;
                } else {
                    m_results.scrollAtBottom = true;
                }
                break;
            }
            // if we didn't break then we must be at the bottom
            m_results.scrollAtBottom = true;
        }
        m_term.setFgColour(oldFgColour, mode);
    }
}

void Ui::displayMenu(terminal::OutputMode mode)
{
    const std::size_t topRow = m_termSize.rows - m_menuRowSize;
    hr(topRow, mode);
    if (m_commandSeqCount > 0) {
        m_term.printAt(m_termSize.rows - 1, 0, ":", mode);
        m_term.saveCursorPosition(mode);
    }
    m_term.printAt(topRow, 1, "Menu", mode);
    m_menu.printMenu(topRow + 1, 1, mode);
    if (m_commandSeqCount > 0) {
        m_term.restoreCursorPosition(mode);
        m_term.setCursorType(terminal::CursorType::BlockBlinking, mode);
        m_term.cursorOn(mode);
    } else {
        m_term.cursorOff(mode);
    }
}

void Ui::restart()
{
    // TODO dirty check
    clearResults(terminal::OutputMode::immediate);
    m_clue.searchString.clear();
    m_clue.foundString.clear();
    m_clue.clueNumber.clear();
    m_clue.comment.clear();
    m_clue.dirty = false;
    clearResults();
}

void Ui::hr(std::size_t row, terminal::OutputMode mode)
{
    m_term.goTo(row, 0, mode);
    std::string hr;
    for (std::size_t c = 0; c < m_termSize.cols; ++c) {
        if (m_term.utf8Supported()) {
            hr.append("─"); // UTF-8 line character
        } else {
            hr.append("-"); // plain
        }
    }
    m_term.print(hr, mode);
}

void Ui::jumble()
{
    std::string foundLetters;

    // What are our found letters? Exclude '.' and '/'
    std::ranges::copy_if(m_clue.foundString, std::back_inserter(foundLetters), [](char c) {
        return c != '.' && c != '/';
    });
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
    clearResults();
    m_results.vec.emplace_back("");
    for (const auto& s : grid) {
        m_results.vec.emplace_back(" " + s);
    }
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back("");

    std::string alreadyFound;
    for (const auto& c : m_clue.foundString) {
        if (c == '.') {
            alreadyFound.append("_ ");
        } else {
            alreadyFound.append(std::format("{} ", c));
        }
    }
    m_results.vec.emplace_back(alreadyFound);
}

void Ui::lookup()
{
    terminal::MessageBoxOptions opts;
    opts.row = 8;
    opts.col = 3;
    opts.message = "Searching...";
    m_term.messageBox(opts);
    clearResults();
    std::string lowerCase { m_clue.foundString };
    std::transform(
        m_clue.foundString.begin(),
        m_clue.foundString.end(),
        lowerCase.begin(),
        [](unsigned char c) { return ascii::tolower(c); });
    std::ranges::replace(lowerCase, '/', ' ');
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
    m_results.vec.emplace_back(std::format("  Odd:   {}", utils::everyNth(m_clue.searchString, 2)));
    m_results.vec.emplace_back(
        std::format(
            "  Even:  {}",
            utils::everyNth({ m_clue.searchString.begin() + 1, m_clue.searchString.end() }, 2)));
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back("Every three letters");
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back(std::format("  Odd:   {}", utils::everyNth(m_clue.searchString, 3)));
    m_results.vec.emplace_back(
        std::format(
            "  Even:  {}",
            utils::everyNth({ m_clue.searchString.begin() + 1, m_clue.searchString.end() }, 3)));
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back("Every two letters (reversed)");
    m_results.vec.emplace_back("");
    std::string reverseSearchString { m_clue.searchString };
    std::reverse(reverseSearchString.begin(), reverseSearchString.end());
    m_results.vec.emplace_back(std::format("  Odd:   {}", utils::everyNth(reverseSearchString, 2)));
    m_results.vec.emplace_back(
        std::format(
            "  Even:  {}",
            utils::everyNth({ reverseSearchString.begin() + 1, reverseSearchString.end() }, 2)));
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back("Every three letters (reversed)");
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back(std::format("  Odd:   {}", utils::everyNth(reverseSearchString, 3)));
    m_results.vec.emplace_back(
        std::format(
            "  Even:  {}",
            utils::everyNth({ reverseSearchString.begin() + 1, reverseSearchString.end() }, 3)));
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
        m_results.type = ResultsType::FreeForm;
    }
}

void Ui::load()
{
    if (m_savedClues.empty()) {
        setResults("No saved clues.");
        return;
    }
    clearResults(terminal::OutputMode::immediate);
    appendResults("Saved clues:");
    appendResults("");
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
    displayResults(terminal::OutputMode::immediate);
    terminal::InputOptions opts;
    opts.row = 4;
    opts.col = 10;
    opts.bgColour = terminal::Colour::Grey;
    opts.fgColour = terminal::Colour::BrightWhite;
    opts.mode = terminal::Mode::Insert;
    opts.keysAllowed = terminal::KeysAllowed::CapitalAlphanum;
    opts.maxLen = 4;
    std::string clueNumber = m_term.input(opts);
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
    if (m_clue.clueNumber.empty()) {
        setResults("Please enter a clue number first");
        return;
    }
    m_savedClues[m_clue.clueNumber] = m_clue;
    setResults(std::format("Clue saved as '{}'.", m_clue.clueNumber));
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
    m_term.messageBox(msgboxOpts);
    terminal::InputOptions inputOpts;
    inputOpts.row = m_termSize.rows - 1;
    inputOpts.col = 1;
    inputOpts.keysAllowed = terminal::KeysAllowed::All;
    std::string filter = m_term.input(inputOpts);
    std::transform(filter.begin(), filter.end(), filter.begin(), ascii::tolower);
    std::vector<std::string> newResults;
    std::string regexPrefix;
    if (!filter.contains("^")) {
        regexPrefix = "^.*";
    }
    std::string regexSuffix;
    if (!filter.contains("$")) {
        regexSuffix = "^.*$";
    }
    filter = std::format("{}{}{}", regexPrefix, filter, regexSuffix);
    log(std::format("Filter regex: '{}'", filter));
    const std::regex regex(filter);
    for (const auto& w : m_results.vec) {
        if (std::regex_match(w, regex)) {
            newResults.emplace_back(w);
        }
    }
    setResults(newResults, ResultsType::Words);
    m_results.filtered = true;
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
    log(std::format("argv[0] = {}", bin.string()));
    std::filesystem::path cwd = bin.parent_path();
    for (int n = 0; n < 3; ++n) {
        log(std::format("Searching for data files in {}", cwd.string()));
        if (std::filesystem::exists(cwd / "words_1.txt")) {
            log(std::format("Data files found in {}", cwd.string()));
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
    opts.keysAllowed = terminal::KeysAllowed::All;
    opts.preInsertHook = [&](int key) -> int {
        if (key == keyPress::SPACE) {
            return keyPress::RIGHT;
        }
        if (key == keyPress::CTRL_U) {
            opts.currentValue = std::string(m_clue.searchString.size(), '.');
            opts.maxLen = opts.currentValue.size();
            opts.cursorPos = 0;
            return keyPress::NO_KEY;
        }
        if (key < 0) {
            // disallow extended characters, e.g. 'é'
            return keyPress::NO_KEY;
        }
        if (key == keyPress::BACKSPACE && opts.cursorPos > 0
            && opts.currentValue.at(opts.cursorPos - 1) == '/') {
            --opts.maxLen;
            return key;
        }
        if (key == keyPress::BACKSPACE && opts.cursorPos > 0) {
            --opts.cursorPos;
            opts.currentValue[opts.cursorPos] = '.';
            return keyPress::NO_KEY;
        }
        if (key == keyPress::DELETE && opts.currentValue[opts.cursorPos] == '/') {
            opts.currentValue.erase(opts.cursorPos, 1);
            --opts.maxLen;
            return keyPress::NO_KEY;
        }
        if (key == keyPress::DELETE) {
            opts.currentValue[opts.cursorPos] = '.';
            return keyPress::NO_KEY;
        }
        if (key == '/') {
            // Disallow entry of separator at beginning or end
            if (opts.cursorPos == 0 || opts.cursorPos > opts.currentValue.size() - 1) {
                return keyPress::NO_KEY;
            }
            // Disallow entry of separator immediately next to an existing separator
            if (opts.currentValue.at(opts.cursorPos) == '/'
                || opts.currentValue.at(opts.cursorPos - 1) == '/') {
                return keyPress::NO_KEY;
            }
            ++opts.maxLen;
            return key;
        }
        if (ascii::isprint(key)) {
            // Disallow any character not in search string IF the search string has been set
            if (!m_clue.searchString.empty()) {
                if (ascii::toupper(key) == opts.currentValue.at(opts.cursorPos)) {
                    // we're just overwriting an existing "found" character
                    return ascii::toupper(key);
                }
                auto c1 = std::count(
                    m_clue.searchString.begin(), m_clue.searchString.end(), ascii::toupper(key));
                auto c2 = std::count(
                    opts.currentValue.begin(), opts.currentValue.end(), ascii::toupper(key));
                if (c1 == 0 || c2 == c1) {
                    m_term.bell(terminal::OutputMode::immediate);
                    return keyPress::NO_KEY;
                }
                if (!ascii::isalpha(key)) {
                    m_term.bell(terminal::OutputMode::immediate);
                    return keyPress::NO_KEY;
                } else {
                    return ascii::toupper(key);
                }
            } else {
                return ascii::toupper(key);
            }
        }
        return key;
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
    if (!m_clue.foundString.empty()) {
        // May have been extended with word separators
        opts.maxLen = m_clue.foundString.size();
    } else {
        opts.maxLen = m_clue.searchString.size();
    }
    m_clue.foundString = m_term.input(opts);
    m_clue.dirty = true;
    log(std::format("m_clue.foundString (constrained) input: '{}'", m_clue.foundString));
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
    opts.keysAllowed = terminal::KeysAllowed::All;
    opts.preInsertHook = [&](int key) -> int {
        if (key == ' ') {
            return '.';
        }
        if (key == '/' || key == '.' || ascii::isalpha(key)) {
            return ascii::toupper(key);
        }
        // Because we've specified KeysAllowed::All we need to let these special keys
        // through but not others.
        // TODO: Desperately need a bitset on KeysAllowed
        if (key == keyPress::BACKSPACE || key == keyPress::LEFT || key == keyPress::RIGHT
            || key == keyPress::DELETE || key == keyPress::CTRL_A || key == keyPress::CTRL_E
            || key == keyPress::END || key == keyPress::HOME || key == keyPress::CTRL_U
            || key == keyPress::ENTER || key == keyPress::ESC) {
            return key;
        }
        return keyPress::NO_KEY;
    };
    while (true) {
        m_clue.foundString = m_term.input(opts);
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
    log(std::format("m_clue.foundString (unconstrained) input: '{}'", m_clue.foundString));
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
    opts.keysAllowed = terminal::KeysAllowed::CapitalAlpha;
    opts.reportSize = true;
    opts.preInsertHook = [&](int key) -> int {
        // Disallow spaces
        if (key == ' ') {
            m_term.bell(terminal::OutputMode::immediate);
            return keyPress::NO_KEY;
        }
        if (key < keyPress::NO_KEY) {
            // disallow extended characters
            return keyPress::NO_KEY;
        }
        return key;
    };
    m_clue.searchString = m_term.input(opts);
    if (separatedStringSize(m_clue.foundString) != m_clue.searchString.size()) {
        m_clue.foundString = std::string(m_clue.searchString.size(), '.');
    }
    m_clue.dirty = true;
    log(std::format("m_clue.searchString input: '{}'", m_clue.searchString));
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
    m_clue.comment = m_term.input(opts);
    m_clue.dirty = true;
    log(std::format("m_clue.comment input: '{}'", m_clue.comment));
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
    opts.keysAllowed = terminal::KeysAllowed::CapitalAlphanum;
    opts.maxLen = 4;
    m_clue.clueNumber = m_term.input(opts);
    m_clue.dirty = true;
    log(std::format("m_clue input: '{}'", m_clue.clueNumber));
}

std::size_t Ui::getResultsPaneRowSize()
{
    return m_termSize.rows - m_menuRowSize - m_headerRowSize;
}

} // namespace ui