#include "terminal.h"
#include <iostream>

int main()
{
    terminal::Terminal term;
    term.setFgColour(terminal::Colour::BrightRed);
    term.printAt(2, 2, "Hello world");
    term.setFgColour(terminal::Colour::BrightWhite);
    term.printAt(4, 4, "All your base are belong to us");
    term.setFgColour(terminal::Colour::Grey);
    term.printAt(5,5, "This is grey");
    term.setFgColour(terminal::Colour::Default);
    term.render();
    std::cout << "\nPress a key " << std::flush;
    [[maybe_unused]] char c = term.getChar();
    return 0;
}