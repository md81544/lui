#pragma once

#include <cstddef>
#include <cstdint>
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

class Terminal final {
public:
    Terminal();
    ~Terminal();
    // There can only be one terminal:
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    void render();
    void printAt(std::size_t row, std::size_t col, std::string_view text);
    void print(std::string_view text);
    // Note! ANSI row/cols are 1-based but we use 0-based here
    void goTo(std::size_t row, std::size_t col);
    void setFgColour(Colour colour);
    void setBgColour(Colour colour);
    Colour getFgColour() const;
    Colour getBgColour() const;
    void cursorUp(uint8_t n);
    void cursorDown(uint8_t n);
    void cursorRight(uint8_t n);
    void cursorLeft(uint8_t n);
    void clearToEndOfLine();
    void clearToStartOfLine();
    void clearLine();
    void saveCursorPosition();
    void restoreCursorPosition();
    void cursorOn();
    void cursorOff();
    int getChar(); // Blocking call
    // Note beep will happen when the next render occurs
    void bell();
    // Helper function that automatically highlights any character in the
    // given string which is preceded by an underscore
    void printMenuString(Colour normal, Colour highlight, std::string_view text);
    // Note the size of the terminal can change if the user
    // resizes it, so a safe approach is to query this before each render().
    std::tuple<std::size_t, std::size_t> getTerminalSize() const;
    bool utf8Supported();
    // Save current screen
    void store();
    // Restore saved screen
    void restore();
    // Writes DIRECTLY to the terminal, will disappear on next render().
    // Useful for "Processing, please wait" type messages
    void messageBox(std::size_t row, std::size_t col, std::string_view msg);

private:
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