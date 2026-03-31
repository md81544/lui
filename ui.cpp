#include "ui.h"
#include "keypress.h"
#include "terminal.h"
#include "word_searcher.h"
#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <stdexcept>

namespace {
std::filesystem::path locateDataDirectory(std::string_view argv0)
{
    const std::filesystem::path bin(argv0);
    std::filesystem::path cwd = bin;
    cwd = cwd.parent_path();
    for (int n = 0; n < 3; ++n) {
        if (std::filesystem::exists(cwd / "words_1.txt")) {
            return cwd;
        }
        cwd = cwd.parent_path();
    }
    // If we get here we could not locate the data needed
    throw std::runtime_error("Could not locate data directory");
}
} // anonymous namespace

namespace ui {

Ui::Ui(std::string_view argv0)
{
    const auto dataDir = locateDataDirectory(argv0);
    m_term.printAt(1, 2, "Loading data...");
    m_term.cursorOff();
    m_term.render();
    m_ws = std::make_unique<wordSearcher::WordSearcher>(
        dataDir / "words_1.txt",
        dataDir / "words_2.txt",
        dataDir / "words_3.txt",
        dataDir / "thesaurus.txt",
        dataDir / "definitions.txt");
}

void Ui::checkForTerminalResize()
{
    auto [rows, cols] = m_term.getTerminalSize();
    if (rows < 10 || cols < 20) { // TODO tweak this accordingly when layout is finalised
        throw(std::runtime_error("Terminal size is too small!"));
    }
    m_termSize.rows = rows;
    m_termSize.cols = cols;
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
        switch (keyPress) {
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
    m_term.printAt(1, 1, std::format("Search:  {}", m_searchString));
    m_term.printAt(2, 1, std::format("Found:   {}", m_foundString));
    m_term.printAt(3, 1, std::format("Comment: {}", m_comment));
    m_term.printAt(4, 1, std::format("Clue:    {}", m_clue));
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
    if (m_searchString.empty()) {
        resultsClear();
        m_results.emplace_back("No search string is set");
    }
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

} // namespace ui