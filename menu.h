#pragma once

#include "terminal.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace menu {

struct Item {
    std::optional<int> id;
    std::size_t row;
    std::string text;
    std::size_t displayWidth;
};

class Menu {
public:
    Menu(terminal::Terminal& term);
    ~Menu() = default;
    Menu(const Menu& rhs) = delete;
    Menu& operator=(const Menu& rhs) = delete;
    Menu(Menu&& rhs) = delete;
    Menu& operator=(Menu&& rhs) = delete;

    void addItem(int id, std::string_view text);
    void addNewLine();
    void printMenu(std::size_t row, std::size_t col, terminal::OutputMode mode);
    int getIdFromHitBox(std::size_t clickRow, std::size_t clickCol);

private:
    terminal::Terminal& m_term;
    std::vector<Item> m_items;
    std::size_t m_currentEntryLine { 0 };
    std::size_t m_row;
    std::size_t m_col;
};

} // namespace menu