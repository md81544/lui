#include "terminal.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#ifdef _WIN32
#include <conio.h>
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace terminal {

Terminal::Terminal()
{
#ifdef _WIN32
    if (!_isatty(_fileno(stdout))) {
        m_isTTY = false;
    }
#else
    if (!isatty(STDOUT_FILENO)) {
        m_isTty = false;
    }
#endif
    m_renderString.clear();
    if (m_isTty) {
        std::cout << "\033[?1049h"; // switch to alternate screen
        std::cout << "\033[2J"; // clear screen
        std::cout << "\033[H"; // cursor to home (1,1)
        std::cout << std::flush;
    }
}

Terminal::~Terminal()
{
    if (m_isTty) {
        std::cout << "\033[?1049l" << std::flush; // switch back to normal screen
    }
}

void Terminal::render()
{
    std::cout << m_renderString << std::endl;
    m_renderString.clear();
    if (m_isTty) {
        m_renderString.append("\033[2J\033[H"); // clear screen
    }
}

void Terminal::printAt(std::size_t row, std::size_t col, std::string_view text)
{
    goTo(row, col);
    m_renderString.append(text);
}

void Terminal::print(std::string_view text)
{
    m_renderString.append(text);
}

void Terminal::goTo(std::size_t row, std::size_t col)
{
    // Note this function is 0-based whereas ANSI codes are 1-based
    if (m_isTty) {
        m_renderString.append(
            "\033[" + std::to_string(row + 1) + ";" + std::to_string(col + 1) + "H");
    }
}

void Terminal::setFgColour(Colour colour)
{
    if (m_isTty) {
        m_renderString.append(colourToAnsiFg(colour));
    }
}

void Terminal::setBgColour(Colour colour)
{
    if (m_isTty) {
        m_renderString.append(colourToAnsiFg(colour));
    }
}

void Terminal::cursorUp(uint8_t n)
{
    if (m_isTty) {
        m_renderString.append("\033[" + std::to_string(n) + "A");
    }
}

void Terminal::cursorDown(uint8_t n)
{
    if (m_isTty) {
        m_renderString.append("\033[" + std::to_string(n) + "B");
    }
}

void Terminal::cursorRight(uint8_t n)
{
    if (m_isTty) {
        m_renderString.append("\033[" + std::to_string(n) + "C");
    }
}

void Terminal::cursorLeft(uint8_t n)
{
    if (m_isTty) {
        m_renderString.append("\033[" + std::to_string(n) + "D");
    }
}

void Terminal::clearToEndOfLine()
{
    if (m_isTty) {
        m_renderString.append("\033[K");
    }
}

void Terminal::clearToStartOfLine()
{
    if (m_isTty) {
        m_renderString.append("\033[1K");
    }
}

void Terminal::clearLine()
{
    if (m_isTty) {
        m_renderString.append("\033[2K");
    }
}

void Terminal::saveCursorPosition()
{
    if (m_isTty) {
        m_renderString.append("\033[s");
    }
}

void Terminal::restoreCursorPosition()
{
    if (m_isTty) {
        m_renderString.append("\033[u");
    }
}

char Terminal::getChar()
{
    if (!m_isTty) {
        return 0;
    }
#ifdef _WIN32
    return _getch();
#else
    termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    char c = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return c;
#endif
}

std::string Terminal::colourToAnsiFg(Colour colour)
{
    std::string rc { "\033[" };
    switch (colour) {
        case terminal::Colour::Black:
            rc.append("30m");
            break;
        case terminal::Colour::Red:
            rc.append("31m");
            break;
        case terminal::Colour::Green:
            rc.append("32m");
            break;
        case terminal::Colour::Yellow:
            rc.append("33m");
            break;
        case terminal::Colour::Blue:
            rc.append("34m");
            break;
        case terminal::Colour::Magenta:
            rc.append("35m");
            break;
        case terminal::Colour::Cyan:
            rc.append("36m");
            break;
        case terminal::Colour::White:
            rc.append("37m");
            break;
        case terminal::Colour::Grey:
            rc.append("90m");
            break;
        case terminal::Colour::BrightRed:
            rc.append("91m");
            break;
        case terminal::Colour::BrightGreen:
            rc.append("92m");
            break;
        case terminal::Colour::BrightYellow:
            rc.append("93m");
            break;
        case terminal::Colour::BrightBlue:
            rc.append("94m");
            break;
        case terminal::Colour::BrightMagenta:
            rc.append("95m");
            break;
        case terminal::Colour::BrightCyan:
            rc.append("96m");
            break;
        case terminal::Colour::BrightWhite:
            rc.append("97m");
            break;
        case terminal::Colour::Default:
            rc.append("39m");
            break;
        default:
            assert(false);
            rc.clear();
    }
    return rc;
}

std::string Terminal::colourToAnsiBg(Colour colour)
{
    std::string rc { "\033[" };
    switch (colour) {
        case terminal::Colour::Black:
            rc.append("40m");
            break;
        case terminal::Colour::Red:
            rc.append("41m");
            break;
        case terminal::Colour::Green:
            rc.append("42m");
            break;
        case terminal::Colour::Yellow:
            rc.append("43m");
            break;
        case terminal::Colour::Blue:
            rc.append("44m");
            break;
        case terminal::Colour::Magenta:
            rc.append("45m");
            break;
        case terminal::Colour::Cyan:
            rc.append("46m");
            break;
        case terminal::Colour::White:
            rc.append("47m");
            break;
        case terminal::Colour::Grey:
            rc.append("100m");
            break;
        case terminal::Colour::BrightRed:
            rc.append("101m");
            break;
        case terminal::Colour::BrightGreen:
            rc.append("102m");
            break;
        case terminal::Colour::BrightYellow:
            rc.append("103m");
            break;
        case terminal::Colour::BrightBlue:
            rc.append("104m");
            break;
        case terminal::Colour::BrightMagenta:
            rc.append("105m");
            break;
        case terminal::Colour::BrightCyan:
            rc.append("106m");
            break;
        case terminal::Colour::BrightWhite:
            rc.append("107m");
            break;
        case terminal::Colour::Default:
            rc.append("49m");
            break;
        default:
            assert(false);
            rc.clear();
    }
    return rc;
}

} // namespace terminal