#include "hotkeys.h"
#include "ui.h"
#include <cassert>
#include <optional>
#include <utility>
#include <vector>

namespace hotkeys {

void Hotkeys::add(int keyPress, Type type, const ui::CommandType& ct)
{
    [[maybe_unused]] auto it = m_hotkeys.find(keyPress);
    assert(it == m_hotkeys.end());
    m_hotkeys.insert({keyPress, Kt{ct, type}});
}

std::vector<int> Hotkeys::get(Type type)
{
    std::vector<int> rc;
    for (const auto& pr : m_hotkeys) {
        if (pr.second.type == type) {
            rc.push_back(pr.first);
        }
    }
    return rc;
}

std::optional<ui::Command> Hotkeys::getCommandFromKeyPress(int keyPress)
{
    auto it = m_hotkeys.find(keyPress);
    if (it == m_hotkeys.end()) {
        return std::nullopt;
    }
    return ui::Command(it->second.commandType);
}

} // namespace hotkeys