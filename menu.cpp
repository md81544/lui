#include "menu.h"
#include "terminal.h"
#include <algorithm>
#include <cstddef>

namespace menu {

Menu::Menu(terminal::Terminal& term)
    : m_term { term }
{
}

void Menu::addItem(int id, std::string_view text)
{
    std::size_t displayWidth
        = std::count_if(text.begin(), text.end(), [](char c) -> bool { return c != '_'; });
    m_items.emplace_back(
        Item { id, m_currentEntryLine, { text.begin(), text.end() }, displayWidth });
}

void Menu::addNewLine()
{
    ++m_currentEntryLine;
}

void Menu::printMenu(std::size_t row, std::size_t col, terminal::OutputMode mode)
{
    m_row = row;
    m_col = col;
    std::size_t currentRow = 0;
    m_term.goTo(row, col, mode);
    for (const auto& i : m_items) {
        if (i.row > currentRow) {
            currentRow = i.row;
            m_term.goTo(row + currentRow, col, mode);
        }
        m_term.printMenuString(
            terminal::Colour::Default, terminal::Colour::BrightWhite, i.text + " ", mode);
    }
}

std::optional<int> Menu::getIdFromHitBox(std::size_t clickRow, std::size_t clickCol)
{
    std::size_t currentCol = m_col;
    for (const auto& i : m_items) {
        if (i.row != clickRow - m_row) {
            continue;
        }
        if (clickCol >= currentCol && clickCol  <= currentCol + i.displayWidth) {
            return i.id;
        }
        currentCol += (i.displayWidth + 1);
    }
    return std::nullopt;
}

} // namespace menu
