#include "menu.h"

namespace menu {

void Menu::addItem(int id, std::string_view text)
{
    m_items.emplace_back(Item { id, { text.begin(), text.end() } });
}

void Menu::addItem(Type)
{
    m_items.emplace_back(std::nullopt, "\n");
}

int Menu::getIdFromHitBox(std::size_t row [[maybe_unused]], std::size_t col [[maybe_unused]])
{
    return 0;
}

} // namespace menu
