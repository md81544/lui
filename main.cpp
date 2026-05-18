#include "log.h"
#include "signal_handler.h"
#include "ui.h"
#include <iostream>

int main(int, char** argv)
{
    try {
        mgo::installSignalHander();

#ifndef NDEBUG
        mgo::Log::init("lui.log", mgo::Log::Level::Debug);
        mgo::Log::info("Running DEBUG build");
#else
        mgo::Log::init("lui.log", mgo::Log::Level::Info);
        mgo::Log::info("Running release build");
#endif
        mgo::Log::info("Lui started");

        ui::Ui ui(argv[0]);
        return ui.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}