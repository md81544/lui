#include "ui.h"
#include "keypress.h"
#include "word_searcher.h"
#include <filesystem>
#include <format>
#include <memory>

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
    // This is currently just a test for the Terminal
    // class which formats output (and reads key presses)

    std::size_t row = 10;

    m_term.cursorOn();

    while (true) {

        m_term.setFgColour(terminal::Colour::BrightRed);
        m_term.printAt(2, 2, "Hello world");
        m_term.setFgColour(terminal::Colour::BrightWhite);
        m_term.printAt(4, 4, "All your base are belong to us");
        m_term.setFgColour(terminal::Colour::Grey);
        m_term.printAt(5, 5, "This is grey");
        m_term.setFgColour(terminal::Colour::BrightMagenta);
        m_term.printAt(row, 18, "This is movable (up/down arrow)");

        // Fixed position footer
        m_term.setFgColour(terminal::Colour::Default);
        auto [rows, cols] = m_term.getTerminalSize();
        std::string hr;
        for (std::size_t c = 0; c < cols; ++c) {
            if (m_term.utf8Supported()) {
                hr.append("━");
            } else {
                hr.append("-");
            }
        }

        m_term.printAt(rows - 3, 0, hr);
        m_term.printAt(rows - 2, 0, std::format("Terminal size: {} rows, and {} cols", rows, cols));

        // Helper function to highlight items in string for menus
        m_term.goTo(12, 0);
        m_term.printMenuString(
            terminal::Colour::Yellow,
            terminal::Colour::BrightWhite,
            "_Lookup _Edit _Selection _View _Help");
        m_term.print("\n");
        m_term.setFgColour(terminal::Colour::Default);

        m_term.print("\nSelect option (Esc to quit) ");

        m_term.render();

        int keyPress = m_term.getChar();
        if (keyPress == keyPress::ESC || keyPress == keyPress::CTRL_C) {
            break;
        } else if (keyPress == keyPress::UP) {
            if (row > 0) {
                --row;
            }
        } else if (keyPress == keyPress::DOWN) {
            if (row < rows - 4) {
                ++row;
            }
        } else if (keyPress == keyPress::CTRL_U) {
            m_term.setBgColour(terminal::Colour::BrightGreen);
            m_term.setFgColour(terminal::Colour::Black);
            m_term.printAt(rows - 4, 0, "Ctrl-U was pressed");
            m_term.setBgColour(terminal::Colour::Default);
        } else if (keyPress == 'l' || keyPress == 'L') {
            auto vec = m_ws->regexSearch("c.m..t.r");
            m_term.goTo(16, 0);
            m_term.setFgColour(terminal::Colour::BrightGreen);
            for (const auto& w : vec) {
                m_term.print(std::format("{} ", w));
            }
        } else {
            m_term.bell();
        }
    }
    return 0;
}
