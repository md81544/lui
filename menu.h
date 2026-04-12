#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace menu {

struct Item {
    std::optional<int> id;
    std::string text;
};

enum class Type {
    NewLine,
};

class Menu {
public:
    Menu() = default;
    ~Menu() = default;
    Menu(const Menu& rhs) = delete;
    Menu& operator=(const Menu& rhs) = delete;
    Menu(Menu&& rhs) = delete;
    Menu& operator=(Menu&& rhs) = delete;
    
    void addItem(int id, std::string_view text);
    void addItem(Type);
    int getIdFromHitBox(std::size_t row, std::size_t col);

private:
    std::vector<Item> m_items;
};

} // namespace menu