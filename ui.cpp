#include "ui.h"
#include "keypress.h"
#include "terminal.h"
#include "utils.h"
#include "word_searcher.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <numbers>
#include <random>
#include <stdexcept>
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
            = static_cast<char>(std::toupper(static_cast<unsigned char>(localLetters[i * 2])));
        grid[row2][col2]
            = static_cast<char>(std::toupper(static_cast<unsigned char>(localLetters[i * 2 + 1])));
    }

    return grid;
}

} // anonymous namespace

namespace ui {

Ui::Ui(std::string_view argv0)
{
    log("DEBUG LOG");
    const auto dataDir = locateDataDirectory(argv0);
    m_term.printAt(1, 2, "Loading data...");
#ifndef NDEBUG
    m_term.printAt(3, 2, "*** DEBUG BUILD *** (will be slow)");
#endif
    m_term.cursorOff();
    m_term.render();
    log("Loading data...");
    m_ws = std::make_unique<wordSearcher::WordSearcher>(
        dataDir / "words_1.txt",
        dataDir / "words_2.txt",
        dataDir / "words_3.txt",
        dataDir / "thesaurus.txt",
        dataDir / "definitions.txt");
    log("Finished loading data");
}

void Ui::checkForTerminalResize()
{
    auto [rows, cols] = m_term.getTerminalSize();
    if (rows < 10 || cols < 20) { // TODO tweak this accordingly when layout is finalised
        throw(std::runtime_error("Terminal size is too small!"));
    }
    if (m_termSize.rows != rows || m_termSize.cols != cols) {
        log(std::format("Terminal size is now {} rows by {} cols", rows, cols));
        m_termSize.rows = rows;
        m_termSize.cols = cols;
    }
}

int Ui::run()
{
    bool finished { false };
    while (!finished) {
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
            case keyPress::CTRL_C:
            case 'Q':
            case 'q':
                finished = true;
                break;
            case 'j':
            case 'J':
                jumble();
                break;
            case 'l':
            case 'L':
                lookup();
                break;
            case 'f':
            case 'F':
                {
                    // Enter "found" string
                    terminal::InputOptions opts;
                    opts.mode = terminal::Mode::Overwrite;
                    opts.maxLen = m_searchString.size();
                    opts.defaultValue = m_foundString;
                    opts.row = 2;
                    opts.col = 10;
                    opts.bgColour = terminal::Colour::BrightCyan;
                    opts.fgColour = terminal::Colour::Black;
                    // Because we have a special use case here we allow all entry
                    // and handle it specifically in the callback hook
                    opts.keysAllowed = terminal::KeysAllowed::All;
                    opts.preInsertHook = [&](int key) -> int {
                        if (key < keyPress::NO_KEY) {
                            // disallow extended characters, e.g. 'é'
                            return keyPress::NO_KEY;
                        }
                        if (key >= 32 && key < 127) {
                            // Disallow spaces
                            if (key == ' ') {
                                m_term.bell(terminal::OutputMode::immediate);
                                return keyPress::NO_KEY;
                            }
                            if (key == '/') {
                                ++opts.maxLen;
                                // TODO disallow '/' at beginning, end, or adjacent to another '/'
                            } else {
                                if (!std::isalpha(key)) {
                                    return keyPress::NO_KEY;
                                } else {
                                    return std::toupper(key);
                                }
                            }
                        }
                        return key;
                    };
                    opts.postInsertHook = [&]() -> bool {
                        auto debugtmp = std::string(opts.maxLen - opts.currentValue.size(), '.');
                        opts.currentValue.append(debugtmp);
                        return true;
                    };
                    opts.maxLen = m_searchString.size();
                    m_foundString = m_term.input(opts);
                    log(std::format("m_foundString input: '{}'", m_foundString));
                }
                break;
            case 'c':
            case 'C':
                {
                    // TODO: This is just test code for the new Terminal::input()
                    terminal::InputOptions opts;
                    opts.row = 3;
                    opts.col = 10;
                    opts.bgColour = terminal::Colour::BrightCyan;
                    opts.fgColour = terminal::Colour::Black;
                    opts.mode = terminal::Mode::Overwrite;
                    std::string foo = m_term.input(opts);
                    log(std::format("string entered: '{}'", foo));
                    break;
                }
            case 's':
            case 'S':
                {
                    // Enter "search" string; implies a restart
                    restart();
                    displayHeader(terminal::OutputMode::immediate);
                    terminal::InputOptions opts;
                    opts.defaultValue = m_searchString;
                    opts.row = 1;
                    opts.col = 10;
                    opts.bgColour = terminal::Colour::BrightCyan;
                    opts.fgColour = terminal::Colour::Black;
                    opts.keysAllowed = terminal::KeysAllowed::CapitalAlpha;
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
                break;
            case keyPress::DOWN:
                if (!m_resultsScrollAtBottom) {
                    ++m_resultsScrollOffset;
                }
                break;
            case keyPress::UP:
                if (m_resultsScrollOffset > 0) {
                    --m_resultsScrollOffset;
                }
                break;
            case keyPress::F12:
                resultsClear();
                m_results = m_debugLog;
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

void Ui::resultsClear()
{
    m_results.clear();
    m_resultsScrollOffset = 0;
}

void Ui::resultsSet(const std::vector<std::string>& vec)
{
    m_results = vec;
    m_resultsScrollOffset = 0;
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

void Ui::displayResults()
{
    const std::size_t resultsTopRow = 6;
    std::size_t lastRowInSection = m_termSize.rows - 6; // 6 being the size of the menu section
    hr(resultsTopRow);
    m_term.printAt(resultsTopRow, 1, "Results");

    if (!m_results.empty()) {
        terminal::Colour oldFgColour = m_term.getFgColour();
        m_term.setFgColour(terminal::Colour::BrightYellow);
        std::size_t currentRow = resultsTopRow + 2;
        if (m_resultsScrollOffset != 0) {
            if (m_term.utf8Supported()) {
                m_term.printAt(currentRow - 1, 1, "…");
            } else {
                m_term.printAt(currentRow - 1, 1, "...");
            }
        }
        for (std::size_t p = m_resultsScrollOffset; p < m_results.size(); ++p) {
            if (m_results[p].size() > m_termSize.cols - 2) {
                if (m_term.utf8Supported()) {
                    m_term.printAt(
                        currentRow, 1, m_results[p].substr(0, m_termSize.cols - 3) + "…");
                } else {
                    m_term.printAt(
                        currentRow, 1, m_results[p].substr(0, m_termSize.cols - 5) + "...");
                }
            } else {
                m_term.printAt(currentRow, 1, m_results[p]);
            }
            ++currentRow;
            if (currentRow == lastRowInSection) {
                if (p < m_results.size() - 1) {
                    // It wasn't the last row in m_results
                    if (m_term.utf8Supported()) {
                        m_term.printAt(currentRow, 1, "…");
                    } else {
                        m_term.printAt(currentRow, 1, "...");
                    }
                    m_resultsScrollAtBottom = false;
                } else {
                    m_resultsScrollAtBottom = true;
                }
                break;
            }
            // if we didn't break then we must be at the bottom
            m_resultsScrollAtBottom = true;
        }
        m_term.setFgColour(oldFgColour);
    }
}

void Ui::displayMenu()
{
    const std::size_t menuTopRow = m_termSize.rows - 4;
    hr(menuTopRow);
    m_term.printAt(m_termSize.rows - 4, 1, "Menu");
    m_term.goTo(menuTopRow + 1, 1);
    m_term.printMenuString(
        terminal::Colour::Default,
        terminal::Colour::BrightWhite,
        "_Jumble _Found _Remove _Comment re_Verse re_Gular _Thesaurus");
    m_term.goTo(menuTopRow + 2, 1);
    m_term.printMenuString(
        terminal::Colour::Default,
        terminal::Colour::BrightWhite,
        "_Anagram _Lookup _Define _Note st_Ore r_Etrieve re_Start _Quit");
}

void Ui::restart()
{
    m_searchString.clear();
    m_foundString.clear();
    m_clue.clear();
    m_comment.clear();
    resultsClear();
}

void Ui::hr(std::size_t row)
{
    m_term.goTo(row, 0);
    std::string hr;
    for (std::size_t c = 0; c < m_termSize.cols; ++c) {
        if (m_term.utf8Supported()) {
            hr.append("─"); // UTF-8 line character
        } else {
            hr.append("-"); // plain
        }
    }
    m_term.print(hr);
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
    resultsClear();
    m_results.emplace_back("");
    for (const auto& s : grid) {
        m_results.emplace_back(" " + s);
    }
    m_results.emplace_back("");
    m_results.emplace_back("");

    std::string alreadyFound;
    for (const auto& c : m_foundString) {
        if (c == '.') {
            alreadyFound.append("_ ");
        } else {
            alreadyFound.append(std::format("{} ", c));
        }
    }
    m_results.emplace_back(alreadyFound);
}

void Ui::lookup()
{
    m_term.messageBox(
        8,
        3,
        "Searching...",
        terminal::OutputMode::immediate); // draws immediately, disappears on next
                                          // m_term.render()
    resultsClear();
    std::string lowerCase { m_foundString };
    std::transform(m_foundString.begin(), m_foundString.end(), lowerCase.begin(), ::tolower);
    auto results = m_ws->regexSearch(lowerCase);
    std::string sortedSearchString { m_searchString };
    std::transform(
        sortedSearchString.begin(),
        sortedSearchString.end(),
        sortedSearchString.begin(),
        ::tolower);
    std::ranges::sort(sortedSearchString);
    for (auto word : results) {
        std::string sortedWord { word };
        std::ranges::sort(sortedWord);
        if (sortedWord == sortedSearchString) {
            m_results.push_back(word);
        }
    }
    if (m_results.empty()) {
        resultsSet({ "-- no matches found --" });
    }
}

void Ui::log(std::string_view logEntry [[maybe_unused]])
{
#ifndef NDEBUG
    m_debugLog.push_back(std::format("{}: {}", utils::currentTimeString(), logEntry));
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

} // namespace ui