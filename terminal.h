#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>

// Notes:
//  - This class caches all "prints". Nothing is output to the
//    physical terminal until .render() is called.
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

enum class Mode {
    Insert,
    Overwrite,
};

enum class InputRestriction {
    None,
    CapitalsOnly, // lower case will be converted
    NumericOnly,  // non-numerics will be ignored,
};

struct InputOptions {
    std::size_t row;
    std::size_t col;
    std::size_t maxLen { 0 };
    std::size_t cursorPos { 0 };
    std::string defaultValue {};
    Mode mode { Mode::Insert };
    Colour fgColour { Colour::Default };
    Colour bgColour { Colour::Default };
    // Restriction is for convenience, the caller is free to use
    // more logic in the hook callback.
    InputRestriction restriction { InputRestriction::None };
    // Hook is called after a key is pressed, before it is appended to
    // the input string. Default does nothing. To disallow a key,
    // return keyPress::NO_KEY.
    // Note that opts is passed in to input() by reference, so the
    // caller can modify it as part of the hook if needed.
    std::function<int(int key, std::string_view currentString)> hook {
        [](int key, std::string_view) -> int { return key; }
    };
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
    void clearToEndOfLine(OutputMode mode = OutputMode::render);
    void clearToStartOfLine(OutputMode mode = OutputMode::render);
    void clearLine(OutputMode mode = OutputMode::render);
    void saveCursorPosition(OutputMode mode = OutputMode::render);
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
    void messageBox(
        std::size_t row,
        std::size_t col,
        std::string_view msg,
        OutputMode mode = OutputMode::render);
    // input() is always immediate mode:
    std::string input(InputOptions& opts);

private:
    // if UTF is supported, return utfVersion, otherwise return asciiVersion
    std::string_view utfOrAscii(std::string_view utfVersion, std::string_view asciiVersion);
    // Output either to render string or direct depending on mode parameter:
    void output(std::string_view text, OutputMode mode = OutputMode::render);
    std::string colourToAnsiFg(Colour colour);
    std::string colourToAnsiBg(Colour colour);
    std::string m_renderString;
    std::string m_savedRenderString;
    bool m_isTty { true };
    bool m_utf8Supported { false };
    Colour m_currentFgColour { Colour::Default };
    Colour m_currentBgColour { Colour::Default };
};

} // namespace terminal