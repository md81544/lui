#include "log.h"
#include "ui.h"
#include <iostream>

int main(int, char** argv)
{
    mgo::Log::init("lui.log", mgo::Log::Level::Debug);
    mgo::Log::info("Lui started");
    try {
        ui::Ui ui(argv[0]);
        return ui.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}