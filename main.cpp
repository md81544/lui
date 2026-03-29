#include "keypress.h"
#include "terminal.h"
#include <iostream>

int main()
{
    // This is currently just a test for the Terminal
    // class which formats output (and reads key presses)
    terminal::Terminal term;

    int row = 10;

    while (true) {

        term.setFgColour(terminal::Colour::BrightRed);
        term.printAt(2, 2, "Hello world");
        term.setFgColour(terminal::Colour::BrightWhite);
        term.printAt(4, 4, "All your base are belong to us");
        term.setFgColour(terminal::Colour::Grey);
        term.printAt(5, 5, "This is grey");
        term.setFgColour(terminal::Colour::BrightYellow);
        term.printAt(row, 18, "This is movable (up/down arrow)");

        // Multiple colours in one word:
        // TODO could do with a helper function to parse a string
        term.goTo(8, 1);
        term.setFgColour(terminal::Colour::BrightWhite);
        term.print("M");
        term.setFgColour(terminal::Colour::Default);
        term.print("enu option 1");
        term.cursorDown(3);

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
            if (row < 20) {
                ++row;
            }
        } else {
            term.bell();
        }
    }
    return 0;
}