#pragma once

#include "keypress.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <tuple>

// Notes:
//  - By default, this class caches all "prints". Nothing is output to the
//    physical terminal until .render() is called. Immediate mode is an
//    option however for non-render output.
//  - It is expected that a blocking call (e.g. getChar()) is
//    called once per display loop otherwise the refresh rate
//    will be needlessly fast. If the caller doesn't need to
//    wait for input then it's up to them to introduce a
//    suitable delay between calls to .render().

namespace terminal {

enum class OutputMode {
    render,
    immediate,
};

enum class Colour : std::uint8_t {
    Default,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    Grey,
    BrightRed,
    BrightGreen,
    BrightYellow,
    BrightBlue,
    BrightMagenta,
    BrightCyan,
    BrightWhite,
};

enum class CursorType {
    Default,
    BlockBlinking,
    BlockSteady,
    UnderlineBlinking,
    UnderlineSteady,
    VLineBlinking,
    VlineSteady,
};

enum class Mode {
    Insert,
    Overwrite,
};

enum class InputReportStatus {
    None,
    SizeInLetters,
    Status,
};

class ZHeight {
    friend class ZHeightGuard;

public:
    uint8_t getZHeight()
    {
        return m_currentZHeight;
    }

private:
    // Note private, can only set via ZHeightGuard
    void setZHeight(uint8_t zHeight)
    {
        m_originalZHeight = m_currentZHeight;
        m_currentZHeight = zHeight;
    }
    void restoreZHeight()
    {
        m_currentZHeight = m_originalZHeight;
    }
    uint8_t m_currentZHeight { 0 };
    uint8_t m_originalZHeight { 0 };
};

class ZHeightGuard {
public:
    explicit ZHeightGuard(ZHeight& zh, uint8_t zHeight)
        : m_zh(zh)
    {
        zh.setZHeight(zHeight);
    }
    ~ZHeightGuard()
    {
        m_zh.restoreZHeight();
    }
    ZHeightGuard(const ZHeightGuard&) = delete;
    ZHeightGuard(const ZHeightGuard&&) = delete;
    const ZHeightGuard operator=(const ZHeightGuard&) = delete;
    const ZHeightGuard operator=(const ZHeightGuard&&) = delete;

private:
    ZHeight& m_zh;
};

struct MessageBoxOptions {
    std::size_t row { 0 }; // Top row of box
    std::size_t col { 0 }; // Left-most col of box
    bool alignRight { false }; // if true, col becomes right-most col of box
                               // (text is still left-aligned)
    std::string message; // Use '\n' for multi-line
    bool waitForKey { false };
    std::string prompt; // Prompt to display on bottom row if waiting for key
    OutputMode mode { OutputMode::immediate };
};

// Bitmasks for keys allowed flag for Input Options:
// May be ORed together, e.g. alpha | numeric will allow letters and numbers
// Note these work off ascii.h functions so no locale issues
namespace keysAllowed {
// clang-format off
constexpr uint8_t print   = 0b00000000; // printable characters; the default
constexpr uint8_t alpha   = 0b00000001; // letters
constexpr uint8_t numeric = 0b00000010; // digits
constexpr uint8_t decimal = 0b00000100; // decimal point
constexpr uint8_t punct   = 0b00001000; // punctuation chars
constexpr uint8_t upper   = 0b00010000; // Lower case will be CONVERTED to upper
constexpr uint8_t lower   = 0b00100000; // Upper case will be CONVERTED to lower
constexpr uint8_t special = 0b01000000; // Include any characters in
                                        // InputOptions.specialChars
// clang-format on
}

struct InputOptions {
    std::size_t row { 0 };
    std::size_t col { 0 };
    std::size_t maxLen { 0 };
    std::size_t cursorPos { 0 };
    std::string defaultValue {};
    Mode mode { Mode::Insert };
    CursorType overrideCursorType { CursorType::Default };
    Colour fgColour { Colour::Default };
    Colour bgColour { Colour::Default };
    // Restriction is for convenience, the caller is free to use
    // more logic in the hook callback.
    uint8_t keysAllowed { keysAllowed::print };
    std::string specialKeys {}; // see keysAllowed::special
    // currentValue allows the hook caller to see the current
    // state of the input string (and modify it as required).
    std::string currentValue {};
    // Hooks are called after a key is pressed, before it is inserted to
    // the input string (for pre-) and after it is inserted (for post-).
    // Defaults do nothing. To disallow a key (or cancel the operation
    // in the post hook), return keyPress::NO_KEY.
    // Note that opts is passed in to input() by reference, so the
    // caller can modify it as part of the hook if needed.
    std::function<int(int key)> preInsertHook { [](int key) -> int { return key; } };
    // Note post hook returns a bool. A false return indicates the
    // caller wants to cancel the insertion.
    std::function<bool()> postInsertHook { []() -> bool { return true; } };
    // Appends a status string after the input:
    InputReportStatus reportStatus { InputReportStatus::None };
    std::string statusData; // The caller can manipulate this in hooks and it will
                            // be displayed after the input if reportStatus == Status
    // Tab / shift tab also acts as an "enter" key. The user can
    // check this value after input to determine whether to move
    // to a different field (e.g. if EntryKey == SHIFT_TAB, immediately
    // start input in a previous field). Up to the caller.
    int EntryKey { keyPress::ENTER };
};

class Terminal final {
public:
    Terminal();
    ~Terminal();
    // There can only be one terminal:
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    void render();
    void printAt(
        std::size_t row,
        std::size_t col,
        std::string_view text,
        OutputMode mode = OutputMode::render);
    void print(std::string_view text, OutputMode mode = OutputMode::render);
    // Note! ANSI row/cols are 1-based but we use 0-based here
    void goTo(std::size_t row, std::size_t col, OutputMode mode = OutputMode::render);
    void setFgColour(Colour colour, OutputMode mode = OutputMode::render);
    void setBgColour(Colour colour, OutputMode mode = OutputMode::render);
    Colour getFgColour() const;
    Colour getBgColour() const;
    void cursorUp(uint8_t n, OutputMode mode = OutputMode::render);
    void cursorDown(uint8_t n, OutputMode mode = OutputMode::render);
    void cursorRight(uint8_t n, OutputMode mode = OutputMode::render);
    void cursorLeft(uint8_t n, OutputMode mode = OutputMode::render);
    void styleBold(bool on, OutputMode mode = OutputMode::render);
    void styleItalic(bool on, OutputMode mode = OutputMode::render);
    void styleUnderline(bool on, OutputMode mode = OutputMode::render);
    void noStyle(OutputMode mode = OutputMode::render); // turn off bold, italic, and underline
    void setCursorType(CursorType type, OutputMode = OutputMode::render);
    void clearToEndOfLine(OutputMode mode = OutputMode::render);
    void clearToStartOfLine(OutputMode mode = OutputMode::render);
    void clearLine(OutputMode mode = OutputMode::render);
    void saveCursorPosition(OutputMode mode = OutputMode::render);
    /// Note! restoreCursorPosition will also reset style (e.g. bold).
    /// This is because on many terminals, saving the cursor position
    /// also saves the style, which is restored on a cursor position restore.
    /// Which is surprising unless you are aware of it. So the reset
    /// seems the lesser of two evils.
    void restoreCursorPosition(OutputMode mode = OutputMode::render);
    void cursorOn(OutputMode mode = OutputMode::render);
    void cursorOff(OutputMode mode = OutputMode::render);
    int getChar(); // Blocking call
    void bell(OutputMode mode = OutputMode::render);
    // Helper function that automatically highlights any character in the
    // given string which is preceded by an underscore
    void printMenuString(
        Colour normal,
        Colour highlight,
        std::string_view text,
        OutputMode mode = OutputMode::render);
    // Note the size of the terminal can change if the user
    // resizes it, so a safe approach is to query this before each render().
    std::tuple<std::size_t, std::size_t> getTerminalSize() const;
    bool utf8Supported();
    // Save current screen
    void store();
    // Restore saved screen
    void restore();
    int messageBox(MessageBoxOptions& opts);
    // Passes to keyPress any shutdown function. Function should return true
    // if, say, SIGTERM is received.
    void setShutdownCheckFunction(std::function<bool()>);
    // input() is always immediate mode:
    // NOTE! As it stands, input will clear to end of line
    // as characters are entered. This is deliberate to avoid an
    // issue where tmux displays *pasted* input (which might be
    // longer than a fixed-width input) before we even get a chance
    // to see each individual "key press" from the pasted string.
    std::string input(InputOptions& opts);

    // Get ANSI sequences as strings for inclusion elsewhere, for
    // example if the caller wants to embed in a string.
    std::string getAnsiSequenceBold(bool on);
    std::string getAnsiSequenceItalic(bool on);
    std::string getAnsiSequenceUnderline(bool);
    std::string getAnsiSequenceNoStyle();
    std::string getAnsiSequenceFgColour(Colour colour);
    std::string getAnsiSequenceBgColour(Colour colour);

private:
    // if UTF is supported, return utfVersion, otherwise return asciiVersion
    std::string_view utfOrAscii(std::string_view utfVersion, std::string_view asciiVersion);
    // Output either to render string or direct depending on mode parameter.
    // Note m_zHeight is used to determine z position IF mode is render
    void output(std::string_view text, OutputMode mode = OutputMode::render);
    std::string colourToAnsiFg(Colour colour);
    std::string colourToAnsiBg(Colour colour);
    ZHeight m_zHeight;
    std::multimap<uint8_t, std::string> m_renderMap;
    std::multimap<uint8_t, std::string> m_savedRenderMap;
    bool m_utf8Supported { false };
    Colour m_currentFgColour { Colour::Default };
    Colour m_currentBgColour { Colour::Default };
};

} // namespace terminal