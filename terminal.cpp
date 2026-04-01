#include "terminal.h"
#include "keypress.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <termios.h>
#include <tuple>
#include <unistd.h>

namespace {

bool hasUtf8Support()
{
    for (const char* var : { "LC_ALL", "LC_CTYPE", "LANG" }) {
        const char* val = std::getenv(var);
        if (val && val[0]) {
            std::string_view s(val);
            for (auto c : s) {
                c = toupper(c);
            }
            if (s.find("UTF-8") != std::string_view::npos
                || s.find("UTF8") != std::string_view::npos) {
                return true;
            }
        }
    }
    return false;
}

} // anonymous namespace

namespace terminal {

Terminal::Terminal()
{
    if (!isatty(STDOUT_FILENO)) {
        m_isTty = false;
    }
    if (hasUtf8Support()) {
        m_utf8Supported = true;
    }
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
        std::cout << "\033[?25h"; // cursor unhide in case it was off
        std::cout << "\033[?1049l" << std::flush; // switch back to normal screen
    }
}

void Terminal::render()
{
    std::cout << m_renderString << std::flush;
    m_renderString.clear();
    if (m_isTty) {
        m_renderString.append("\033[2J\033[H"); // clear screen
        setFgColour(Colour::Default);
        setBgColour(Colour::Default);
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
    m_currentFgColour = colour;
    if (m_isTty) {
        m_renderString.append(colourToAnsiFg(colour));
    }
}

void Terminal::setBgColour(Colour colour)
{
    m_currentBgColour = colour;
    if (m_isTty) {
        m_renderString.append(colourToAnsiBg(colour));
    }
}

Colour Terminal::getFgColour() const
{
    return m_currentFgColour;
}

Colour Terminal::getBgColour() const
{
    return m_currentFgColour;
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

void Terminal::cursorOn()
{
    if (m_isTty) {
        m_renderString.append("\033[?25h");
    }
}

void Terminal::cursorOff()
{
    if (m_isTty) {
        m_renderString.append("\033[?25l");
    }
}

int Terminal::getChar()
{
    if (!m_isTty) {
        return keyPress::ESC;
    }
    auto key = keyPress::getKeyPress();
    // key should never been nullopt because
    // we didn't call getKeyPress() in non-blocking mode
    assert(key.has_value());
    return key.value_or(keyPress::NO_KEY);
}

void Terminal::bell()
{
    if (m_isTty) {
        m_renderString.append("\007");
    }
}

void Terminal::printMenuString(Colour normal, Colour highlight, std::string_view text)
{
    bool highlighting { false };
    setFgColour(normal);
    for (const char c : text) {
        if (c == '_') {
            highlighting = true;
            setFgColour(highlight);
        } else {
            print(std::string { c });
            if (highlighting) {
                highlighting = false;
                setFgColour(normal);
            }
        }
    }
}

std::tuple<std::size_t, std::size_t> Terminal::getTerminalSize() const
{
    winsize ws;
    unsigned short rows { 0 };
    unsigned short cols { 0 };
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        rows = ws.ws_row;
        cols = ws.ws_col;
    }
    return { rows, cols };
}

bool Terminal::utf8Supported()
{
    return m_utf8Supported;
}

void Terminal::store()
{
    m_renderString = m_savedRenderString;
}

void Terminal::restore()
{
    m_savedRenderString = m_renderString;
}

void Terminal::messageBox(std::size_t row, std::size_t col, std::string_view msg)
{
    if (!m_isTty) {
        return;
    }
    // Go to position:
    std::cout << "\033[" + std::to_string(row + 1) + ";" + std::to_string(col + 1) + "H";
    if (utf8Supported()) {
        std::cout << "┌─";
    } else {
        std::cout << "+-";
    }
    for (std::size_t n = 0; n < msg.size(); ++n) {
        if (utf8Supported()) {
            std::cout << "─";
        } else {
            std::cout << "-";
        }
    }
    if (utf8Supported()) {
        std::cout << "─┐";
    } else {
        std::cout << "-+";
    }
    // next row
    std::cout << "\033[" + std::to_string(row + 2) + ";" + std::to_string(col + 1) + "H";
    if (utf8Supported()) {
        std::cout << "│ " << msg << " │";
    } else {
        std::cout << "| " << msg << " |";
    }
    // next row
    std::cout << "\033[" + std::to_string(row + 3) + ";" + std::to_string(col + 1) + "H";
    if (utf8Supported()) {
        std::cout << "└─";
    } else {
        std::cout << "+-";
    }
    for (std::size_t n = 0; n < msg.size(); ++n) {
        if (utf8Supported()) {
            std::cout << "─";
        } else {
            std::cout << "-";
        }
    }
    if (utf8Supported()) {
        std::cout << "─┘";
    } else {
        std::cout << "-+";
    }
    std::cout << std::flush;
}

// Private member functions:

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
