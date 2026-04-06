#include <algorithm>
#include <string_view>
#include <vector>

// Super simple command-line parser.
// Usage:
// int main(int argc, char** argv) {
//    CommandLineParser clp(argc, argv);
//
//    if (clp.has("--verbose")) { ... }
//    if (clp.has("-v")) { ... }
//}

class CommandLineParser {
public:
    CommandLineParser(int argc, char** argv)
        : m_args(argv, argv + argc)
    {
    }

    [[nodiscard]] bool has(std::string_view flag) const
    {
        return std::ranges::any_of(m_args, [flag](std::string_view arg) { return arg == flag; });
    }

private:
    std::vector<std::string_view> m_args;
};