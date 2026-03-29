#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <string>
#include <tuple>

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
    void render();
    void printAt(std::size_t row, std::size_t col, std::string_view text);
    void print(std::string_view text);
    // Note! ANSI row/cols are 1-based but we use 0-based here
    void goTo(std::size_t row, std::size_t col);
    void setFgColour(Colour colour);
    void setBgColour(Colour colour);
    void cursorUp(uint8_t n);
    void cursorDown(uint8_t n);
    void cursorRight(uint8_t n);
    void cursorLeft(uint8_t n);
    void clearToEndOfLine();
    void clearToStartOfLine();
    void clearLine();
    void saveCursorPosition();
    void restoreCursorPosition();
    int  getChar();
    // Note beep will happen when the next render occurs
    void bell();
    // Helper function that automatically highlights any character in the
    // given string which is preceded by an underscore
    void printMenuString(Colour normal, Colour highlight, std::string_view text);
    // Note the size of the terminal can change if the user
    // resizes it, so a safe approach is to query this before each render().
    std::tuple<std::size_t, std::size_t> getTerminalSize();

private:
    std::string colourToAnsiFg(Colour colour);
    std::string colourToAnsiBg(Colour colour);
    std::string m_renderString;
    bool m_isTty{true};
};

} //namespace terminal