#include "cpp-terminal/color.hpp"
#include "cpp-terminal/cursor.hpp"
#include "cpp-terminal/screen.hpp"

#include <iostream>

int main() {
    std::cout << Term::clear_screen();
    std::cout << Term::cursor_move(2,2) <<
        Term::color_fg(Term::Color::Name::Red) << "Hello world\n" << std::endl;
    return 0;
}