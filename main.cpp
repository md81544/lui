#include "terminal.h"
#include <iostream>

int main()
{
    // This is currently just a test for the Terminal
    // class which formats output.

    terminal::Terminal term;

    term.setFgColour(terminal::Colour::BrightRed);
    term.printAt(2, 2, "Hello world");
    term.setFgColour(terminal::Colour::BrightWhite);
    term.printAt(4, 4, "All your base are belong to us");
    term.setFgColour(terminal::Colour::Grey);
    term.printAt(5,5, "This is grey");

    // Multiple colours in one word:
    term.goTo(8, 1);
    term.setFgColour(terminal::Colour::BrightWhite);
    term.print("M");
    term.setFgColour(terminal::Colour::Default);
    term.print("enu option 1");
    term.cursorDown(3);

    term.render();
    std::cout << "Press a key " << std::flush;
    [[maybe_unused]] char c = term.getChar();
    return 0;
}