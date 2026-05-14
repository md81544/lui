#pragma once

// Note, these are locale-independent, used deliberately
// to avoid locale issues if the user's locale is set
// to something other than expected
// The functions take ints to cope with extended character input
// without sanitisation errors

namespace ascii {

constexpr bool isascii(int c) noexcept
{
    return c <= 0x7F;
}

constexpr bool isdigit(int c) noexcept
{
    return c >= '0' && c <= '9';
}

constexpr bool islower(int c) noexcept
{
    return c >= 'a' && c <= 'z';
}

constexpr bool isupper(int c) noexcept
{
    return c >= 'A' && c <= 'Z';
}

constexpr bool isalpha(int c) noexcept
{
    return islower(c) || isupper(c);
}

constexpr bool isalnum(int c) noexcept
{
    return isalpha(c) || isdigit(c);
}

constexpr bool isxdigit(int c) noexcept
{
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

constexpr bool isspace(int c) noexcept
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

constexpr bool isprint(int c) noexcept
{
    return c >= 0x20 && c <= 0x7E;
}

constexpr bool iscntrl(int c) noexcept
{
    return c <= 0x1F || c == 0x7F;
}

constexpr char tolower(int c) noexcept
{
    return isupper(c) ? static_cast<char>(c + ('a' - 'A')) : static_cast<char>(c);
}

constexpr char toupper(int c) noexcept
{
    return islower(c) ? static_cast<char>(c - ('a' - 'A')) : static_cast<char>(c);
}

constexpr bool iequal(int a, int b) noexcept
{
    return tolower(a) == tolower(b);
}

constexpr bool ispunct(int c)
{
    if (!isprint(c)) {
        return false;
    }
    return !(isdigit(c) || isalpha(c));
}

} // namespace ascii