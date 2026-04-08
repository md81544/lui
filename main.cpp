#include "command_line_parser.h"
#include "log.h"
#include "signal_handler.h"
#include "ui.h"
#include <iostream>

int main(int argc, char** argv)
{
    mgo::installSignalHander();

    int wordComplexity = 3; // use words_3.txt
    CommandLineParser clp(argc, argv);
    if (clp.has("-w1")) { // Use words_1.txt
        wordComplexity = 1;
    }
    if (clp.has("-w2")) { // use words_2.txt
        wordComplexity = 2;
    }
    if (clp.has("-h") || clp.has("--help")) {
        std::cout << "Usage: lui [-w1|-w2] (word complexity, defaults to 3=most words)\n";
        return 2;
    }

#ifndef NDEBUG
    mgo::Log::init("lui.log", mgo::Log::Level::Debug);
    mgo::Log::info("Running DEBUG build");
#else
    mgo::Log::init("lui.log", mgo::Log::Level::Info);
    mgo::Log::info("Running release build");
#endif
    mgo::Log::info("Lui started");

    try {
        ui::Ui ui(argv[0], wordComplexity);
        return ui.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}