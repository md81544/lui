#pragma once

// Note, these are locale-independent, used deliberately
// to avoid locale issues if the user's locale is set
// to something other than expected

namespace ascii {

constexpr bool isascii(unsigned char c) noexcept
{
    return c <= 0x7F;
}

constexpr bool isdigit(unsigned char c) noexcept
{
    return c >= '0' && c <= '9';
}

constexpr bool islower(unsigned char c) noexcept
{
    return c >= 'a' && c <= 'z';
}

constexpr bool isupper(unsigned char c) noexcept
{
    return c >= 'A' && c <= 'Z';
}

constexpr bool isalpha(unsigned char c) noexcept
{
    return islower(c) || isupper(c);
}

constexpr bool isalnum(unsigned char c) noexcept
{
    return isalpha(c) || isdigit(c);
}

constexpr bool isxdigit(unsigned char c) noexcept
{
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

constexpr bool isspace(unsigned char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

constexpr bool isprint(unsigned char c) noexcept
{
    return c >= 0x20 && c <= 0x7E;
}

constexpr bool iscntrl(unsigned char c) noexcept
{
    return c <= 0x1F || c == 0x7F;
}

constexpr char tolower(unsigned char c) noexcept
{
    return isupper(c) ? static_cast<char>(c + ('a' - 'A')) : static_cast<char>(c);
}

constexpr char toupper(unsigned char c) noexcept
{
    return islower(c) ? static_cast<char>(c - ('a' - 'A')) : static_cast<char>(c);
}

constexpr bool iequal(unsigned char a, unsigned char b) noexcept
{
    return tolower(a) == tolower(b);
}

constexpr bool ispunct(unsigned char c)
{
    if (!isprint(c)) {
        return false;
    }
    return !(isdigit(c) || isalpha(c));
}

} // namespace ascii