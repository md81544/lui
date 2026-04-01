#include "ui.h"
#include "keypress.h"
#include "terminal.h"
#include "utils.h"
#include "word_searcher.h"

#include <cctype>
#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <stdexcept>

namespace ui {

Ui::Ui(std::string_view argv0)
{
    log("DEBUG LOG");
    const auto dataDir = locateDataDirectory(argv0);
    m_term.printAt(1, 2, "Loading data...");
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
        log(std::format("Terminal size changed to {} rows by {} cols", rows, cols));
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
        displayCurrentInput(); // call this just before render() as this sets the cursor position
        m_term.render();
        int keyPress = m_term.getChar();
        log(std::format("Key press: {}", keyPress));
        keyPress = inputHandleKeyPress(keyPress);
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
                // Enter "found" string
                input(2, 10, m_foundString, [&]() { m_foundString = m_currentInput.value; });
                break;
            case 's':
            case 'S':
                // Enter "search" string
                input(1, 10, m_searchString, [&]() {
                    m_searchString = m_currentInput.value;
                    if (m_foundString.empty()) {
                        m_foundString = std::string(m_searchString.length(), '.');
                    }
                });
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

void Ui::displayHeader()
{
    m_term.goTo(1, 1);
    m_term.printMenuString(terminal::Colour::Default, terminal::Colour::BrightWhite, "_Search : ");
    m_term.printAt(1, 10, m_searchString);
    m_term.goTo(2, 1);
    m_term.printMenuString(terminal::Colour::Default, terminal::Colour::BrightWhite, "_Found  : ");
    m_term.printAt(2, 10, m_foundString);
    m_term.goTo(3, 1);
    m_term.printMenuString(terminal::Colour::Default, terminal::Colour::BrightWhite, "_Comment: ");
    m_term.printAt(3, 10, m_comment);
    m_term.goTo(4, 1);
    m_term.printMenuString(terminal::Colour::Default, terminal::Colour::BrightWhite, "Clue _No: ");
    m_term.printAt(4, 10, m_clue);
}

void Ui::displayCurrentInput()
{
    if (!m_currentInput.active) {
        m_term.cursorOff();
        return;
    }
    terminal::Colour oldColdBgColour = m_term.getBgColour();
    terminal::Colour oldColdFgColour = m_term.getFgColour();
    m_term.setBgColour(terminal::Colour::BrightCyan);
    m_term.setFgColour(terminal::Colour::Black);
    m_term.printAt(m_currentInput.displayAtRow, m_currentInput.displayAtCol, m_currentInput.value);
    m_term.setBgColour(oldColdBgColour);
    m_term.setFgColour(oldColdFgColour);
    m_term.cursorOn();
    m_term.goTo(
        m_currentInput.displayAtRow, m_currentInput.displayAtCol + m_currentInput.cursorPos);
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

void Ui::input(
    std::size_t row,
    std::size_t col,
    std::string defaultValue,
    std::function<void()> callback,
    bool upperCase /* = true */,
    std::size_t maxSize /* = 0 */)
{
    m_currentInput.active = true;
    m_currentInput.value = defaultValue;
    m_currentInput.cursorPos = 0;
    m_currentInput.displayAtRow = row;
    m_currentInput.displayAtCol = col;
    m_currentInput.maxSize = maxSize;
    m_currentInput.callback = callback;
    m_currentInput.upperCaseOnly = upperCase;
}

int Ui::inputHandleKeyPress(int key)
{
    if (!m_currentInput.active) {
        return key;
    }
    // If we don't handle the key here we return it for higher-level handling
    // e.g. Ctrl-C

    // Currently just letters for testing... TODO expand handled keys
    if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z')) {
        if (m_currentInput.upperCaseOnly) {
            key = toupper(key);
        }
        // We need to handle insertion point (actually overwrite point) TODO
        m_currentInput.value += key;
        ++m_currentInput.cursorPos;
        return keyPress::NO_KEY; // signifies we've swallowed this key
    } else {
        switch (key) {
            case keyPress::ENTER:
                m_currentInput.callback();
                m_currentInput.active = false;
                return keyPress::NO_KEY;
            case keyPress::ESC:
                m_currentInput.active = false;
                return keyPress::NO_KEY;
            case keyPress::BACKSPACE:
                // TODO
                return keyPress::NO_KEY;
            case keyPress::SPACE:
                // Currently disallowed
                return keyPress::NO_KEY;
            case keyPress::LEFT:
                // TODO move cursor
                return keyPress::NO_KEY;
            case keyPress::RIGHT:
                // TODO move cursor
                return keyPress::NO_KEY;
            case keyPress::CTRL_E:
                // TODO move to end of line
                return keyPress::NO_KEY;
            case keyPress::CTRL_A:
                // TODO move to start of line
                return keyPress::NO_KEY;
            case keyPress::CTRL_U:
                // TODO clear entry
                return keyPress::NO_KEY;
            default:
                // do nothing; key returned below
        }
    }
    return key;
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
    resultsClear();
    m_results.emplace_back("Jumble not implemented yet");
}

void Ui::lookup()
{
    // TODO this is currently for testing scrolling!
    std::vector<std::string> vec { "This is test data!",
                                   "apple",
                                   "banana",
                                   "cherry",
                                   "date",
                                   "elderberry",
                                   "fig",
                                   "grape",
                                   "honeydew",
                                   "kiwi",
                                   "lemon",
                                   "mango",
                                   "nectarine",
                                   "orange",
                                   "papaya",
                                   "quince",
                                   "raspberry",
                                   "strawberry",
                                   "tangerine",
                                   "ugli",
                                   "vanilla",
                                   "watermelon",
                                   "xigua",
                                   "yam",
                                   "zucchini",
                                   "apricot",
                                   "blueberry",
                                   "cantaloupe",
                                   "dragonfruit",
                                   "eggplant",
                                   "fennel",
                                   "Lorem ipsum dolor sit amet, consectetur adipiscing elit sed \
do eiusmod tempor incididunt ut labore et dolore magna aliqua",
                                   "guava",
                                   "huckleberry",
                                   "ivory",
                                   "jackfruit",
                                   "kumquat",
                                   "lime",
                                   "mulberry",
                                   "nutmeg",
                                   "olive",
                                   "peach",
                                   "pear",
                                   "plum",
                                   "pomegranate",
                                   "quinoa",
                                   "radish",
                                   "spinach",
                                   "turnip",
                                   "ugni",
                                   "vine",
                                   "walnut" };
    resultsSet(vec);
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
    const std::filesystem::path bin  = std::filesystem::canonical(argv0);
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