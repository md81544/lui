#include "keypress.h"
#include "terminal.h"
#include <cstddef>
#include <format>
#include <iostream>

int main()
{
    // This is currently just a test for the Terminal
    // class which formats output (and reads key presses)
    terminal::Terminal term;

    std::size_t row = 10;

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
        term.printAt(rows - 2, 0, std::format("Terminal size: {} rows, and {} cols", rows, cols));

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
}