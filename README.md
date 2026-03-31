# Lui - Interactive Crossword Helper
This is a C++ rewrite (in progress) of https://github.com/md81544/lookup
but with a TUI interface as standard.

**Points of note:**
* This is a work in progress!
* No dependencies so far. I originally was going to use cpp-terminal for terminal handling, but on inspection the code was horrible (and ncurses felt like overkill) so I've implemented the basic terminal handling required in terminal.{cpp,h}. These files will be useful if you want basic no-frills ANSI terminal handling. It supports:
    * Cursor positioning
    * Printing at a position
    * Clearing the screen
    * Bell
    * get keypress (either blocking or non-blocking)
    * checks for UTF-8 support
    * and more - see terminal.h
* It uses a Makefile to drive CMake. You can just type `make` and it will do the usual `mkdir build && cd build && cmake .. && make` - much easier to type!
    * You can do `make`, `make debug` (same as `make`), `make release`, and `make clean`.