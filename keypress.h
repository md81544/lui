#pragma once

#include <optional>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

namespace {

// Returns true if a byte is available on stdin within timeout_ms milliseconds
bool stdinReady(int timeout_ms)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
}

int readByte()
{
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        return c;
    }
    return -1;
}

struct TerminalGuard {
    termios saved_attrs;
    TerminalGuard()
    {
        tcgetattr(STDIN_FILENO, &saved_attrs);
    }
    ~TerminalGuard()
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_attrs);
    }
};

} // anonymous namespace

namespace keyPress {

// The following are not "special" keys (i.e.
// their values are below 128) but included
// for readability. Add others as needed.
constexpr int NO_KEY = 0;
constexpr int CTRL_A = 1; // Move to input beginning
constexpr int CTRL_B = 2; // "Back" i.e. page up
constexpr int CTRL_C = 3; // Ctrl-C is disabled with ISIG flag but program can quit if it sees this
constexpr int CTRL_E = 5; // Move to input end
constexpr int CTRL_F = 6; // "Forward" i.e. page down
constexpr int TAB = 9;
constexpr int ENTER = 13;
constexpr int CTRL_U = 21; // clear input
constexpr int ESC = 27;
constexpr int SPACE = 43;
constexpr int BACKSPACE = 127;

// Special keys (arbitrary values):
constexpr int UP = 256;
constexpr int DOWN = 257;
constexpr int RIGHT = 258;
constexpr int LEFT = 259;
constexpr int HOME = 260;
constexpr int END = 261;
constexpr int PGUP = 262;
constexpr int PGDN = 263;
constexpr int INSERT = 264;
constexpr int DELETE = 265;
constexpr int F1 = 266;
constexpr int F2 = 267;
constexpr int F3 = 268;
constexpr int F4 = 269;
constexpr int F5 = 270;
constexpr int F6 = 271;
constexpr int F7 = 272;
constexpr int F8 = 273;
constexpr int F9 = 274;
constexpr int F10 = 275;
constexpr int F11 = 276;
constexpr int F12 = 277;
constexpr int UNKNOWN = 1024;

// If called with blocking = false then returns
// nullopt if no keypress is in the input queue
inline std::optional<int> getKeyPress(bool blocking = true)
{
    TerminalGuard terminalGuard; // RAII to save/reset term attrs
    char c;
    termios term_attrs;
    tcgetattr(STDIN_FILENO, &term_attrs);
    term_attrs.c_lflag &= ~(ICANON | ECHO | ISIG);
    if (!blocking) {
        term_attrs.c_cc[VMIN] = 0;
        term_attrs.c_cc[VTIME] = 0;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &term_attrs);
    c = readByte();
    if (!blocking && c == 0) {
        return std::nullopt;
    }
    if (c == 27) {
        // Use a short timeout to distinguish bare ESC from a sequence
        if (stdinReady(50)) {
            int c2 = readByte();
            if (c2 == '[') {
                // CSI sequence — read parameter/intermediate bytes
                // Accumulate until we hit the final byte (0x40–0x7E)
                char seq[16];
                int len = 0;
                while (len < (int)sizeof(seq) - 1) {
                    if (!stdinReady(50)) {
                        break;
                    }
                    int ch = readByte();
                    seq[len++] = (char)ch;
                    if (ch >= 0x40 && ch <= 0x7E) {
                        break; // final byte
                    }
                }
                seq[len] = '\0';

                // Decode common sequences
                if (seq[0] == 'A') {
                    return UP;
                } else if (seq[0] == 'B') {
                    return DOWN;
                } else if (seq[0] == 'C') {
                    return RIGHT;
                } else if (seq[0] == 'D') {
                    return LEFT;
                } else if (seq[0] == 'H') {
                    return HOME;
                } else if (seq[0] == 'F') {
                    return END;
                } else if (seq[0] == '2' && seq[1] == '~') {
                    return INSERT;
                } else if (seq[0] == '3' && seq[1] == '~') {
                    return DELETE;
                } else if (seq[0] == '5' && seq[1] == '~') {
                    return PGUP;
                } else if (seq[0] == '6' && seq[1] == '~') {
                    return PGDN;
                }
                // F1–F4 (xterm style via CSI O...)  handled below
                // F1–F4 (vt100 CSI [ A–D — rare)
                else if (seq[0] == '1' && seq[1] == '1' && seq[2] == '~') {
                    return F1;
                } else if (seq[0] == '1' && seq[1] == '2' && seq[2] == '~') {
                    return F2;
                } else if (seq[0] == '1' && seq[1] == '3' && seq[2] == '~') {
                    return F3;
                } else if (seq[0] == '1' && seq[1] == '4' && seq[2] == '~') {
                    return F4;
                } else if (seq[0] == '1' && seq[1] == '5' && seq[2] == '~') {
                    return F5;
                } else if (seq[0] == '1' && seq[1] == '7' && seq[2] == '~') {
                    return F6;
                } else if (seq[0] == '1' && seq[1] == '8' && seq[2] == '~') {
                    return F7;
                } else if (seq[0] == '1' && seq[1] == '9' && seq[2] == '~') {
                    return F8;
                } else if (seq[0] == '2' && seq[1] == '0' && seq[2] == '~') {
                    return F9;
                } else if (seq[0] == '2' && seq[1] == '1' && seq[2] == '~') {
                    return F10;
                } else if (seq[0] == '2' && seq[1] == '3' && seq[2] == '~') {
                    return F11;
                } else if (seq[0] == '2' && seq[1] == '4' && seq[2] == '~') {
                    return F12;
                } else {
                    return UNKNOWN;
                }

            } else if (c2 == 'O') {
                // SS3 sequence — used for F1-F4 in many terminals
                if (!stdinReady(50)) {
                    return UNKNOWN;
                }
                int c3 = readByte();
                switch (c3) {
                    case 'P':
                        return F1;
                    case 'Q':
                        return F2;
                    case 'R':
                        return F3;
                    case 'S':
                        return F4;
                    case 'H':
                        return HOME; // some terminals
                    case 'F':
                        return END; // some terminals
                    default:
                        return UNKNOWN;
                }
            } else {
                return UNKNOWN;
            }
        }
        // No follow-up byte — bare ESC
        return 27;
    }
    return c;
}

}