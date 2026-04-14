#include "terminal.h"
#include "ascii.h" // for locale-independent functions
#include "keypress.h"
#include "log.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <limits>
#include <ranges>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <termios.h>
#include <tuple>
#include <unistd.h>
#include <vector>

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
        std::cout << "\033[?1000h\033[?1006h"; // SGR mode (if supported) for mouse reporting
        std::cout << "\033[?1049h"; // switch to alternate screen
        std::cout << "\033[2J"; // clear screen
        std::cout << "\033[H"; // cursor to home (1,1)
        std::cout << std::flush;
    }
}

Terminal::~Terminal()
{
    if (m_isTty) {
        mgo::Log::debug("Resetting terminal in ~Terminal()");
        std::cout << "\033[?25h"; // cursor unhide in case it was off
        setCursorType(CursorType::Default, OutputMode::immediate); // reset any cursor change
        std::cout << "\033[?1049l" << std::flush; // switch back to normal screen
        std::cout << "\033[?1000l\033[?1006l"; // exit SGR mode
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

void Terminal::setCursorType(CursorType type, OutputMode mode)
{
    if (!m_isTty) {
        return;
    }
    output("\033[", mode);
    switch (type) {
        case CursorType::Default:
            output("0", mode);
            break;
        case CursorType::BlockBlinking:
            output("1", mode);
            break;
        case CursorType::BlockSteady:
            output("2", mode);
            break;
        case CursorType::UnderlineBlinking:
            output("3", mode);
            break;
        case CursorType::UnderlineSteady:
            output("4", mode);
            break;
        case CursorType::VLineBlinking:
            output("5", mode);
            break;
        case CursorType::VlineSteady:
            output("6", mode);
            break;
        default:
            output("0", mode);
            assert("Unhandled CursorType enum");
    }
    output(" q", mode);
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

int Terminal::messageBox( MessageBoxOptions& opts)
{
    if (opts.waitForKey && opts.mode != OutputMode::immediate) {
        opts.waitForKey = false;
        mgo::Log::error("waitForKey == true doesn't make sense in non-immediate mode");
    }
    if (!m_isTty) {
        return keyPress::NO_KEY;
    }
    std::size_t localRow { opts.row };
    std::size_t maxLen = 0;
    std::vector<std::string> msgRows;
    for (auto subrange : opts.message | std::views::split('\n')) {
        msgRows.emplace_back(subrange.begin(), subrange.end());
        if (maxLen < subrange.size()) {
            maxLen = subrange.size();
        }
    }
    if (maxLen < opts.prompt.size() + 2) {
        maxLen = opts.prompt.size() + 2;
    }
    goTo(localRow, opts.col, opts.mode);
    output(utfOrAscii("┌─", "+-"), opts.mode);
    for (std::size_t n = 0; n < maxLen; ++n) {
        output(utfOrAscii("─", "-"), opts.mode);
    }
    output(utfOrAscii("─┐", "-+"), opts.mode);
    for (const auto& l : msgRows) {
        ++localRow;
        goTo(localRow, opts.col, opts.mode);
        std::string s
            = std::format("{} {:{}} {}", utfOrAscii("│", "|"), l, maxLen, utfOrAscii("│", "|"));
        output(s, opts.mode);
    }
    if (opts.waitForKey) {
        ++localRow;
        goTo(localRow, opts.col, opts.mode);
        saveCursorPosition(opts.mode);
        std::string s
            = std::format("{} {:{}} {}", utfOrAscii("│", "|"), "", maxLen, utfOrAscii("│", "|"));
        output(s, opts.mode);
    }
    goTo(++localRow, opts.col, opts.mode);
    output(utfOrAscii("└─", "+-"), opts.mode);
    for (std::size_t n = 0; n < maxLen; ++n) {
        output(utfOrAscii("─", "-"), opts.mode);
    }
    output(utfOrAscii("─┘", "-+"), opts.mode);
    if (opts.waitForKey) {
        restoreCursorPosition(opts.mode);
        cursorRight(2, opts.mode);
        output(opts.prompt, opts.mode);
        cursorRight(1, opts.mode);
        setCursorType(CursorType::BlockBlinking, opts.mode);
        cursorOn(opts.mode);
        int key = getChar();
        cursorOff(opts.mode);
        return key;
    } else {
        return keyPress::NO_KEY;
    }
}

void Terminal::setShutdownCheckFunction(std::function<bool()> fn)
{
    keyPress::shutdownCheckFunction = fn;
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
    opts.currentValue = opts.defaultValue;
    Colour oldFg = getFgColour();
    Colour oldBg = getBgColour();
    cursorOn(imm);
    bool done = false;
    if (opts.mode == Mode::Insert) {
        setCursorType(CursorType::VLineBlinking, imm);
    } else {
        setCursorType(CursorType::BlockBlinking, imm);
    }
    while (!done) {
        // Print the current value
        goTo(opts.row, opts.col, imm);
        setBgColour(opts.bgColour, imm);
        setFgColour(opts.fgColour, imm);
        std::cout << opts.currentValue;
        // If it's a fixed size then we print underscores
        // to show available space for entry
        if (opts.currentValue.size() < opts.maxLen) {
            for (std::size_t n = 0; n < opts.maxLen - opts.currentValue.size(); ++n) {
                std::cout << "_";
            }
        }
        setFgColour(oldFg, imm);
        setBgColour(oldBg, imm);
        if (opts.reportSize && !opts.currentValue.empty()) {
            std::cout << std::format("  ({} letters)", opts.currentValue.size());
        }
        clearToEndOfLine(imm); // See note in header; this works around tmux behaviour;
                               // Downside is anything after the input will be cleared.
        // Position cursor to insertion/overwrite point
        goTo(opts.row, opts.col + opts.cursorPos, imm);
        int key = getChar();
        if (ascii::isprint(key)) {
            switch (opts.keysAllowed) {
                case terminal::KeysAllowed::All:
                    // Nothing to do, all keys allowed
                    break;
                case terminal::KeysAllowed::Alphanum:
                    if (!ascii::isalnum(key)) {
                        key = keyPress::NO_KEY;
                    }
                    break;
                case terminal::KeysAllowed::CapitalAlphanum:
                    if (ascii::isalnum(key)) {
                        key = ascii::toupper(key);
                    } else {
                        key = keyPress::NO_KEY;
                    }
                    break;
                case terminal::KeysAllowed::Numeric:
                    if (!ascii::isdigit(key)) {
                        key = keyPress::NO_KEY;
                    }
                    break;
                case terminal::KeysAllowed::CapitalAlpha:
                    if (ascii::isalpha(key)) {
                        key = ascii::toupper(key);
                    } else {
                        key = keyPress::NO_KEY;
                    }
                    break;
                default:
                    // Unhandled enum
                    assert(false);
            }
        }
        // Call any pre hook the caller set:
        key = opts.preInsertHook(key);
        // Handle all the "special" keys.
        // We set key to NO_KEY and only restore it
        // in the default: section of the switch below so
        // that key is automatically NO_KEY if handled.
        int keyOrig = key;
        key = keyPress::NO_KEY;
        switch (keyOrig) {
            case keyPress::NO_KEY:
                break;
            case keyPress::ENTER:
                done = true;
                break;
            case keyPress::BACKSPACE:
                if (!opts.currentValue.empty() && opts.cursorPos > 0) {
                    if (opts.cursorPos == opts.currentValue.size()) {
                        opts.currentValue.pop_back();
                    } else {
                        if (opts.cursorPos > 0) {
                            opts.currentValue.erase(
                                opts.currentValue.begin() + opts.cursorPos - 1,
                                opts.currentValue.begin() + opts.cursorPos);
                        }
                    }
                    --opts.cursorPos;
                }
                break;
            case keyPress::DELETE:
                if (opts.cursorPos < opts.currentValue.size()) {
                    opts.currentValue.erase(
                        opts.currentValue.begin() + opts.cursorPos,
                        opts.currentValue.begin() + opts.cursorPos + 1);
                }
                break;
            case keyPress::ESC:
            case keyPress::CTRL_C:
                opts.currentValue = opts.defaultValue;
                done = true;
                break;
            case keyPress::CTRL_A:
            case keyPress::HOME:
                opts.cursorPos = 0;
                break;
            case keyPress::CTRL_E:
            case keyPress::END:
                if (opts.mode == Mode::Overwrite) {
                    // Don't move cursor outside "box" in overwrite mode
                    opts.cursorPos = opts.currentValue.size() - 1;
                } else {
                    opts.cursorPos = opts.currentValue.size();
                }
                break;
            case keyPress::CTRL_U:
                // need to clear the display, not using clear to end
                // of line as it might invalidate other parts of the UI
                goTo(opts.row, opts.col, imm);
                for (std::size_t n = 0; n < opts.currentValue.size(); ++n) {
                    std::cout << " ";
                }
                opts.currentValue.clear();
                opts.cursorPos = 0;
                break;
            case keyPress::LEFT:
                if (opts.cursorPos > 0) {
                    --opts.cursorPos;
                }
                break;
            case keyPress::RIGHT:
                {
                    std::size_t max = opts.currentValue.size();
                    if (opts.mode == Mode::Overwrite && max > 0) {
                        --max;
                    }
                    if (opts.cursorPos < max) {
                        ++opts.cursorPos;
                    }
                }
                break;
            default:
                // key was not handled so reset key to keyOrig
                // only if it is printable (i.e. not an unhandled
                // "special" key)
                if (ascii::isprint(keyOrig)) {
                    key = keyOrig;
                } else {
                    key = keyPress::NO_KEY;
                }
        }
        // Finally add/insert to value
        std::string oldValue = opts.currentValue; // for restoration if caller cancels in post hook
        if (!done && key != keyPress::NO_KEY) {
            std::size_t localMaxLen = opts.maxLen;
            if (localMaxLen == 0) {
                localMaxLen = std::numeric_limits<std::size_t>::max();
            }
            if (opts.cursorPos == opts.currentValue.size()) {
                if (opts.currentValue.size() < localMaxLen) {
                    opts.currentValue.push_back(key);
                }
            } else {
                if (opts.mode == Mode::Overwrite) {
                    opts.currentValue[opts.cursorPos] = key;
                } else {
                    if (opts.currentValue.size() < localMaxLen) {
                        opts.currentValue.insert(opts.currentValue.begin() + opts.cursorPos, key);
                    }
                }
            }
            if ((opts.mode == Mode::Insert && opts.cursorPos < localMaxLen)
                || (opts.mode == Mode::Overwrite && opts.cursorPos < localMaxLen - 1)) {
                ++opts.cursorPos;
            }
        }
        // Call any post hook the caller set. Unless the caller returned NO_KEY
        // in the pre hook.
        if (key != keyPress::NO_KEY) {
            if (!opts.postInsertHook()) {
                // false signifies the caller wants to abort the insertion
                opts.currentValue = oldValue;
            }
        }
    }
    cursorOff(imm);
    return opts.currentValue;
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
