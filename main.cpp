#include "keypress.h"
#include "terminal.h"
#include "word_searcher.h"
#include <cstddef>
#include <filesystem>
#include <format>
#include <iostream>
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

int main(int, char** argv)
{
    try {
        terminal::Terminal term;
        term.printAt(1, 2, "Loading data...");
        term.cursorOff();
        term.render();

        auto dataDir = locateDataDirectory(argv[0]);
        WordSearcher ws(
            dataDir / "words_1.txt",
            dataDir / "words_2.txt",
            dataDir / "words_3.txt",
            dataDir / "thesaurus.txt",
            dataDir / "definitions.txt");

        // This is currently just a test for the Terminal
        // class which formats output (and reads key presses)

        std::size_t row = 10;

        term.cursorOn();

        while (true) {

            term.setFgColour(terminal::Colour::BrightRed);
            term.printAt(2, 2, "Hello world");
            term.setFgColour(terminal::Colour::BrightWhite);
            term.printAt(4, 4, "All your base are belong to us");
            term.setFgColour(terminal::Colour::Grey);
            term.printAt(5, 5, "This is grey");
            term.setFgColour(terminal::Colour::BrightMagenta);
            term.printAt(row, 18, "This is movable (up/down arrow)");

            // Fixed position footer
            term.setFgColour(terminal::Colour::Default);
            auto [rows, cols] = term.getTerminalSize();
            std::string hr;
            for (std::size_t c = 0; c < cols; ++c) {
                hr.append("━");
            }

            term.printAt(rows - 3, 0, hr);
            term.printAt(
                rows - 2, 0, std::format("Terminal size: {} rows, and {} cols", rows, cols));

            // Helper function to highlight items in string for menus
            term.goTo(12, 0);
            term.printMenuString(
                terminal::Colour::Yellow,
                terminal::Colour::BrightWhite,
                "_File _Edit _Selection _View _Help");
            term.print("\n");
            term.setFgColour(terminal::Colour::Default);

            term.render();
            std::cout << "Press a key (Esc to quit) " << std::flush;
            int keyPress = term.getChar();
            if (keyPress == keyPress::ESC) {
                break;
            } else if (keyPress == keyPress::UP) {
                if (row > 0) {
                    --row;
                }
            } else if (keyPress == keyPress::DOWN) {
                if (row < rows - 4) {
                    ++row;
                }
            } else {
                term.bell();
            }
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}