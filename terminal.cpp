#include "terminal.h"
#include "keypress.h"
#include <cassert>
#include <cctype>
#include <cstdint>
#include <format>
#include <iostream>
#include <limits>
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

void Terminal::printAt(std::size_t row, std::size_t col, std::string_view text, OutputMode mode)
{
    goTo(row, col, mode);
    output(text, mode);
}

void Terminal::print(std::string_view text, OutputMode mode)
{
    output(text, mode);
}

void Terminal::goTo(std::size_t row, std::size_t col, OutputMode mode)
{
    // Note this function is 0-based whereas ANSI codes are 1-based
    std::string text = std::format("\033[{};{}H", row + 1, col + 1);
    if (m_isTty) {
        output(text, mode);
    }
}

void Terminal::setFgColour(Colour colour, OutputMode mode)
{
    m_currentFgColour = colour;
    if (m_isTty) {
        output(colourToAnsiFg(colour), mode);
    }
}

void Terminal::setBgColour(Colour colour, OutputMode mode)
{
    m_currentBgColour = colour;
    if (m_isTty) {
        output(colourToAnsiBg(colour), mode);
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

void Terminal::cursorUp(uint8_t n, OutputMode mode)
{
    if (m_isTty) {
        output("\033[" + std::to_string(n) + "A", mode);
    }
}

void Terminal::cursorDown(uint8_t n, OutputMode mode)
{
    if (m_isTty) {
        output("\033[" + std::to_string(n) + "B", mode);
    }
}

void Terminal::cursorRight(uint8_t n, OutputMode mode)
{
    if (m_isTty) {
        output("\033[" + std::to_string(n) + "C", mode);
    }
}

void Terminal::cursorLeft(uint8_t n, OutputMode mode)
{
    if (m_isTty) {
        output("\033[" + std::to_string(n) + "D", mode);
    }
}

void Terminal::clearToEndOfLine(OutputMode mode)
{
    if (m_isTty) {
        output("\033[K", mode);
    }
}

void Terminal::clearToStartOfLine(OutputMode mode)
{
    if (m_isTty) {
        output("\033[1K", mode);
    }
}

void Terminal::clearLine(OutputMode mode)
{
    if (m_isTty) {
        output("\033[2K", mode);
    }
}

void Terminal::saveCursorPosition(OutputMode mode)
{
    if (m_isTty) {
        output("\033[s", mode);
    }
}

void Terminal::restoreCursorPosition(OutputMode mode)
{
    if (m_isTty) {
        output("\033[u", mode);
    }
}

void Terminal::cursorOn(OutputMode mode)
{
    if (m_isTty) {
        output("\033[?25h", mode);
    }
}

void Terminal::cursorOff(OutputMode mode)
{
    if (m_isTty) {
        output("\033[?25l", mode);
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

void Terminal::bell(OutputMode mode)
{
    if (m_isTty) {
        output("\007", mode);
    }
}

void Terminal::printMenuString(
    Colour normal,
    Colour highlight,
    std::string_view text,
    OutputMode mode)
{
    bool highlighting { false };
    setFgColour(normal, mode);
    for (const char c : text) {
        if (c == '_') {
            highlighting = true;
            setFgColour(highlight, mode);
        } else {
            print(std::string { c }, mode);
            if (highlighting) {
                highlighting = false;
                setFgColour(normal, mode);
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

void Terminal::messageBox(std::size_t row, std::size_t col, std::string_view msg, OutputMode mode)
{
    if (!m_isTty) {
        return;
    }
    goTo(row, col, mode);
    output(utfOrAscii("┌─", "+-"), mode);
    for (std::size_t n = 0; n < msg.size(); ++n) {
        output(utfOrAscii("─", "-"), mode);
    }
    output(utfOrAscii("─┐", "-+"), mode);
    goTo(row + 1, col, mode);
    std::string s = std::format("{} {} {}", utfOrAscii("│", "|"), msg, utfOrAscii("│", "|"));
    output(s, mode);
    goTo(row + 2, col, mode);
    output(utfOrAscii("└─", "+-"), mode);
    for (std::size_t n = 0; n < msg.size(); ++n) {
        output(utfOrAscii("─", "-"), mode);
    }
    output(utfOrAscii("─┘", "-+"), mode);
}

std::string Terminal::input(InputOptions& opts)
{
    // Note, not using readline library here. While readline (GNU especially)
    // has hooks to probably cover needs, it IS a separate dependency which
    // may or may not be available when compiling (and will need -I locations
    // determining, especially if installed by homebrew). This should suffice.
    if (!m_isTty) {
        return std::string {};
    }
    constexpr OutputMode imm = OutputMode::immediate;
    std::string value { opts.defaultValue };
    Colour oldFg = getFgColour();
    Colour oldBg = getBgColour();
    cursorOn(imm);
    bool done = false;
    while (!done) {
        // Print the current value
        goTo(opts.row, opts.col, imm);
        setBgColour(opts.bgColour, imm);
        setFgColour(opts.fgColour, imm);
        std::cout << value;
        // If it's a fixed size then we print underscores
        // to show available space for entry
        if (value.size() < opts.maxLen) {
            for (std::size_t n = 0; n < opts.maxLen - value.size(); ++n) {
                std::cout << "_";
            }
        }
        setFgColour(oldFg, imm);
        setBgColour(oldBg, imm);
        std::cout << " "; // in case of backspace to overwrite what's left behind
        // Position cursor to insertion/overwrite point
        goTo(opts.row, opts.col + opts.cursorPos, imm);
        int key = getChar();
        if (opts.restriction == InputRestriction::NumericOnly && ::isalpha(key)
            && !::isdigit(key)) {
            key = keyPress::NO_KEY;
        }
        if (opts.restriction == InputRestriction::CapitalsOnly && ::isalpha(key)) {
            key = ::toupper(key);
        }
        // Call any hook the caller set:
        key = opts.hook(key, value);
        // Handle all the non-ascii keys, e.g. Esc or Ctrl-C
        switch (key) {
            case keyPress::ENTER:
                done = true;
                break;
            case keyPress::BACKSPACE:
                if (!value.empty() && opts.cursorPos > 0) {
                    if (opts.cursorPos == value.size()) {
                        value.pop_back();
                    } else {
                        if (opts.cursorPos > 0) {
                            value.erase(
                                value.begin() + opts.cursorPos - 1, value.begin() + opts.cursorPos);
                        }
                    }
                    --opts.cursorPos;
                }
                break;
            case keyPress::DELETE:
                if (opts.cursorPos < value.size()) {
                    value.erase(value.begin() + opts.cursorPos, value.begin() + opts.cursorPos + 1);
                }
                break;
            case keyPress::ESC:
            case keyPress::CTRL_C:
                value = opts.defaultValue;
                done = true;
                break;
            case keyPress::CTRL_A:
            case keyPress::HOME:
                opts.cursorPos = 0;
                break;
            case keyPress::CTRL_E:
            case keyPress::END:
                opts.cursorPos = value.size();
                break;
            case keyPress::CTRL_U:
                // need to clear the display, not using clear to end
                // of line as it might invalidate other parts of the UI
                goTo(opts.row, opts.col, imm);
                for (std::size_t n = 0; n < value.size(); ++n) {
                    std::cout << " ";
                }
                value.clear();
                opts.cursorPos = 0;
                break;
            case keyPress::LEFT:
                if (opts.cursorPos > 0) {
                    --opts.cursorPos;
                }
                break;
            case keyPress::RIGHT:
                if (opts.cursorPos < value.size()) {
                    ++opts.cursorPos;
                }
                break;
            default:
                // do nothing
        }
        // Finally add/insert to value
        if (!done && key != keyPress::NO_KEY && ::isalnum(key)) {
            std::size_t localMaxLen = opts.maxLen;
            if (localMaxLen == 0) {
                localMaxLen = std::numeric_limits<std::size_t>::max();
            }
            if (opts.cursorPos == value.size()) {
                if (value.size() < localMaxLen) {
                    value.push_back(key);
                }
            } else {
                if (opts.mode == Mode::Overwrite) {
                    value[opts.cursorPos] = key;
                } else {
                    if (value.size() < localMaxLen) {
                        value.insert(value.begin() + opts.cursorPos, key);
                    }
                }
            }
            if (opts.cursorPos < localMaxLen) {
                ++opts.cursorPos;
            }
        }
    }
    cursorOff(imm);
    return value;
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

std::string_view Terminal::utfOrAscii(std::string_view utfVersion, std::string_view asciiVersion)
{
    if (m_utf8Supported) {
        return utfVersion;
    }
    return asciiVersion;
}

void Terminal::output(std::string_view text, OutputMode mode)
{
    if (mode == OutputMode::immediate) {
        std::cout << text << std::flush;
    } else {
        m_renderString.append(text);
    }
}

} // namespace terminal
