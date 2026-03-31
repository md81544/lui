#include "ui.h"
#include "keypress.h"
#include "terminal.h"
#include "word_searcher.h"
#include <filesystem>
#include <format>
#include <memory>
#include <stdexcept>

namespace {
std::filesystem::path locateDataDirectory(std::string_view argv0)
{
    std::filesystem::path bin(argv0);
    std::filesystem::path cwd = bin.remove_filename();
    for (int n = 0; n < 4; ++n) {
        if (std::filesystem::exists(cwd / "words_1.txt")) {
            return cwd;
        }
        cwd = cwd.parent_path();
    }
    // If we get here we could not locate the data needed
    throw std::runtime_error("Could not locate data directory");
}
} // anonymous namespace

Ui::Ui(std::string_view argv0)
{
    auto dataDir = locateDataDirectory(argv0);
    m_term.printAt(1, 2, "Loading data...");
    m_term.cursorOff();
    m_term.render();
    m_ws = std::make_unique<WordSearcher>(
        dataDir / "words_1.txt",
        dataDir / "words_2.txt",
        dataDir / "words_3.txt",
        dataDir / "thesaurus.txt",
        dataDir / "definitions.txt");
}

int Ui::run()
{
    bool finished { false };
    while (!finished) {
        // Check for terminal resize
        auto [rows, cols] = m_term.getTerminalSize();
        if (rows < 10 || cols < 20) { // TODO tweak this accordingly when layout is finalised
            throw(std::runtime_error("Terminal size is too small!"));
        }
        m_rows = rows;
        m_cols = cols;
        displayHeader();
        displayMenu();
        m_term.render();
        int keyPress = m_term.getChar();
        switch (keyPress) {
            case keyPress::CTRL_C:
            case 'Q':
            case 'q':
                finished = true;
                break;
            default:
                m_term.bell();
        }
    }
    return 0;
}

void Ui::displayHeader()
{
    m_term.printAt(1, 1, std::format("Search:  {}", m_searchString));
    m_term.printAt(2, 1, std::format("Found:   {}", m_foundString));
    m_term.printAt(3, 1, std::format("Comment: {}", m_comment));
    m_term.printAt(4, 1, std::format("Clue:    {}", m_clue));
    hr(5);
}

void Ui::displayMenu()
{
    hr(m_rows - 3);
    m_term.printAt(m_rows - 3, 1, "Menu");
    m_term.goTo(m_rows - 2, 1);
    m_term.printMenuString(
        terminal::Colour::Default,
        terminal::Colour::BrightWhite,
        "_Jumble _Found _Remove _Comment re_Verse re_Gular _Thesaurus");
    m_term.goTo(m_rows - 1, 1);
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
    m_results.clear();
}

void Ui::hr(std::size_t row)
{
    m_term.goTo(row, 0);
    std::string hr;
    for (std::size_t c = 0; c < m_cols; ++c) {
        if (m_term.utf8Supported()) {
            hr.append("─"); // UTF-8 line character
        } else {
            hr.append("-"); // plain
        }
    }
    m_term.print(hr);
}
