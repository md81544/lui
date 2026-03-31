#include "ui.h"
#include <iostream>

int main(int, char** argv)
{
    try {
        ui::Ui ui(argv[0]);
        return ui.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}