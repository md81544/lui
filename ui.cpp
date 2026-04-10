#include "ui.h"
#include "ascii.h"
#include "keypress.h"
#include "log.h"
#include "signal_handler.h"
#include "terminal.h"
#include "utils.h"
#include "word_searcher.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <numbers>
#include <random>
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
        log(std::format("Key press: {}", keyPress));
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
            case 'l':
            case 'L':
                if (m_commandSeqCount > 0) {
                    // i.e. :l
                    load();
                } else {
                    lookup();
                }
                break;
            case 'd':
            case 'D':
                if (m_results.type == ResultsType::Words) {
                    m_results.vec = m_ws->definitions(m_results.vec);
                    m_results.type = ResultsType::FreeForm;
                }
                break;
            case 'f':
            case 'F':
                if (m_searchString.empty()) {
                    enterFoundStringUnconstrained();
                } else {
                    enterFoundStringConstrained();
                }
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
                pageDownResults();
                break;
            case keyPress::PGUP:
            case keyPress::CTRL_B: // Note Ctrl-B may be TMux's "prefix" key!
                pageUpResults();
                break;
            case keyPress::F12:
                clearResults();
                setResults(m_debugLog, ResultsType::FreeForm);
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
    if (mode == terminal::OutputMode::immediate) {
        // If it's immediate we want to clear the results pane:
        for (size_t r = m_resultsTopRow + 1; r < m_resultsTopRow + getResultsPaneRowSize(); ++r) {
            m_term.goTo(r, 0, terminal::OutputMode::immediate);
            m_term.clearLine(terminal::OutputMode::immediate);
        }
    }
}

void Ui::setResults(const std::vector<std::string>& vec, ResultsType type)
{
    clearResults(terminal::OutputMode::immediate);
    m_results.vec = vec;
    m_results.type = type;
    m_results.scrollOffset = 0;
}

void Ui::setResults(std::string_view result, ResultsType type)
{
    clearResults(terminal::OutputMode::immediate);
    m_results.vec.clear();
    m_results.vec.emplace_back(result);
    m_results.type = type;
    m_results.scrollOffset = 0;
}

void Ui::displayHeader(terminal::OutputMode mode)
{
    // Can use immediate mode to clear the header before an input (which is immediate)
    m_term.goTo(1, 1, mode);
    m_term.printMenuString(
        terminal::Colour::Default, terminal::Colour::BrightWhite, "_Search : ", mode);
    if (!m_searchString.empty()) {
        m_term.printAt(
            1, 10, std::format("{}  ({} letters)", m_searchString, m_searchString.size()), mode);
    }
    m_term.clearToEndOfLine(mode);
    m_term.goTo(2, 1, mode);
    m_term.printMenuString(
        terminal::Colour::Default, terminal::Colour::BrightWhite, "_Found  : ", mode);
    m_term.clearToEndOfLine(mode);
    m_term.printAt(2, 10, m_foundString, mode);
    m_term.clearToEndOfLine(mode);
    m_term.goTo(3, 1, mode);
    m_term.printMenuString(
        terminal::Colour::Default, terminal::Colour::BrightWhite, "_Comment: ", mode);
    m_term.clearToEndOfLine(mode);
    m_term.printAt(3, 10, m_comment, mode);
    m_term.clearToEndOfLine(mode);
    m_term.goTo(4, 1, mode);
    m_term.printMenuString(
        terminal::Colour::Default, terminal::Colour::BrightWhite, "Clue _No: ", mode);
    m_term.printAt(4, 10, m_clue, mode);
    m_term.clearToEndOfLine(mode);
}

void Ui::displayResults(terminal::OutputMode mode)
{
    std::size_t lastRowInSection = m_resultsTopRow + getResultsPaneRowSize() - 1;
    hr(m_resultsTopRow, mode);
    m_term.printAt(m_resultsTopRow, 1, "Results", mode);

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
    m_term.goTo(topRow + 1, 1, mode);
    m_term.printMenuString(
        terminal::Colour::Default,
        terminal::Colour::BrightWhite,
        "_Jumble _Comment re_Verse _Regular _Thesaurus",
        mode);
    m_term.goTo(topRow + 2, 1, mode);
    m_term.printMenuString(
        terminal::Colour::Default,
        terminal::Colour::BrightWhite,
        "_Lookup _Define _^_Save _^_Load _^_Restart _^_Quit",
        mode);
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
    clearResults(terminal::OutputMode::immediate);
    m_searchString.clear();
    m_foundString.clear();
    m_clue.clear();
    m_comment.clear();
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
    std::ranges::copy_if(m_foundString, std::back_inserter(foundLetters), [](char c) {
        return c != '.' && c != '/';
    });
    // Which letters are remaining in the search string?
    std::unordered_map<char, int> freq;
    for (char c : foundLetters) {
        ++freq[c];
    }
    std::string remainingLetters;
    std::ranges::copy_if(m_searchString, std::back_inserter(remainingLetters), [&freq](char c) {
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
    for (const auto& c : m_foundString) {
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
    m_term.messageBox(
        8,
        3,
        "Searching...",
        terminal::OutputMode::immediate); // draws immediately, disappears on next
                                          // m_term.render()
    clearResults();
    std::string lowerCase { m_foundString };
    std::transform(
        m_foundString.begin(), m_foundString.end(), lowerCase.begin(), [](unsigned char c) {
            return ascii::tolower(c);
        });
    std::ranges::replace(lowerCase, '/', ' ');
    auto results = m_ws->regexSearch(lowerCase);
    if (!m_searchString.empty()) {
        std::string sortedSearchString { m_searchString };
        std::transform(
            sortedSearchString.begin(),
            sortedSearchString.end(),
            sortedSearchString.begin(),
            [](unsigned char c) { return ascii::tolower(c); });
        std::ranges::sort(sortedSearchString);
        for (const auto& word : results) {
            // Ensure that any regex match actually contains the letters
            // in m_searchString
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
        setResults("-- no matches found --", ResultsType::FreeForm);
    } else {
        m_results.type = ResultsType::Words;
    }
}

void Ui::regular()
{
    clearResults();
    if (m_searchString.empty()) {
        setResults("Please enter a search string first", ResultsType::FreeForm);
        return;
    }
    m_results.vec.emplace_back("Every two letters");
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back(std::format("  Odd:   {}", utils::everyNth(m_searchString, 2)));
    m_results.vec.emplace_back(
        std::format(
            "  Even:  {}",
            utils::everyNth({ m_searchString.begin() + 1, m_searchString.end() }, 2)));
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back("Every three letters");
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back(std::format("  Odd:   {}", utils::everyNth(m_searchString, 3)));
    m_results.vec.emplace_back(
        std::format(
            "  Even:  {}",
            utils::everyNth({ m_searchString.begin() + 1, m_searchString.end() }, 3)));
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back("Every two letters (reversed)");
    m_results.vec.emplace_back("");
    std::string reverseSearchString { m_searchString };
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
    if (m_searchString.empty()) {
        setResults("Please enter a search string first", ResultsType::FreeForm);
        return;
    }
    std::string reversed { m_searchString };
    std::reverse(reversed.begin(), reversed.end());
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back(std::format("'{}' reversed is:", m_searchString));
    m_results.vec.emplace_back("");
    m_results.vec.emplace_back(reversed);
}

void Ui::load()
{
    setResults("Load not yet implemented", ResultsType::FreeForm);
}

void Ui::save() {
    setResults("Save not yet implemented", ResultsType::FreeForm);
}

void Ui::pageDownResults()
{
    std::size_t resultsDisplaySize = getResultsPaneRowSize() - 4;
    if (m_results.scrollOffset + resultsDisplaySize < m_results.vec.size()) {
        m_results.scrollOffset += resultsDisplaySize;
    }
}

void Ui::pageUpResults()
{
    std::size_t resultsDisplaySize = getResultsPaneRowSize() - 4;
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

void Ui::enterFoundStringConstrained()
{
    terminal::InputOptions opts;
    opts.mode = terminal::Mode::Overwrite;
    opts.defaultValue = m_foundString;
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
            opts.currentValue = std::string(m_searchString.size(), '.');
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
            if (!m_searchString.empty()) {
                if (ascii::toupper(key) == opts.currentValue.at(opts.cursorPos)) {
                    // we're just overwriting an existing "found" character
                    return ascii::toupper(key);
                }
                auto c1
                    = std::count(m_searchString.begin(), m_searchString.end(), ascii::toupper(key));
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
    if (!m_foundString.empty()) {
        // May have been extended with word separators
        opts.maxLen = m_foundString.size();
    } else {
        opts.maxLen = m_searchString.size();
    }
    m_foundString = m_term.input(opts);
    log(std::format("m_foundString (constrained) input: '{}'", m_foundString));
}

void Ui::enterFoundStringUnconstrained()
{
    displayHeader(terminal::OutputMode::immediate);
    terminal::InputOptions opts;
    opts.defaultValue = m_foundString;
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
        m_foundString = m_term.input(opts);
        if (m_foundString.starts_with('/') || m_foundString.ends_with('/')) {
            // TODO need an immediate messagebox with "press any key"
            opts.defaultValue = m_foundString;
            setResults(
                "Found string cannot start with or end with a separator ('/')",
                ResultsType::FreeForm);
            displayResults(terminal::OutputMode::immediate);
            opts.cursorPos = 0;
            continue;
        }
        if (m_foundString.contains("//")) {
            // TODO need an immediate messagebox with "press any key"
            opts.defaultValue = m_foundString;
            setResults(
                "Found string cannot contain two or more consecutive separators ('/')",
                ResultsType::FreeForm);
            displayResults(terminal::OutputMode::immediate);
            opts.cursorPos = 0;
            continue;
        }
        clearResults(terminal::OutputMode::immediate);
        break;
    }
    log(std::format("m_foundString (unconstrained) input: '{}'", m_foundString));
}

void Ui::enterSearchString()
{
    displayHeader(terminal::OutputMode::immediate);
    terminal::InputOptions opts;
    opts.defaultValue = m_searchString;
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
    m_searchString = m_term.input(opts);
    m_foundString = std::string(m_searchString.size(), '.');
    log(std::format("m_searchString input: '{}'", m_searchString));
}

void Ui::enterCommentString()
{
    terminal::InputOptions opts;
    opts.row = 3;
    opts.col = 10;
    opts.bgColour = terminal::Colour::Grey;
    opts.fgColour = terminal::Colour::BrightWhite;
    opts.mode = terminal::Mode::Insert;
    opts.defaultValue = m_comment;
    m_comment = m_term.input(opts);
    log(std::format("m_comment input: '{}'", m_comment));
}

void Ui::enterClueNumber()
{
    terminal::InputOptions opts;
    opts.row = 4;
    opts.col = 10;
    opts.bgColour = terminal::Colour::Grey;
    opts.fgColour = terminal::Colour::BrightWhite;
    opts.mode = terminal::Mode::Insert;
    opts.defaultValue = m_clue;
    opts.keysAllowed = terminal::KeysAllowed::CapitalAlphanum;
    m_clue = m_term.input(opts);
    log(std::format("m_clue input: '{}'", m_clue));
}

std::size_t Ui::getResultsPaneRowSize()
{
    return m_termSize.rows - m_menuRowSize - m_headerRowSize;
}

} // namespace ui