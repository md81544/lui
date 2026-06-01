#pragma once

// This class serves as a central repository for all "hot keys", i.e.
// menu and other keypress commands

#include <flat_map>
#include <optional>
#include <vector>

namespace ui {
enum class CommandType;
struct Command;
}

namespace hotkeys {

enum class Type {
    Menu,
    Global,
};

struct Kt {
    ui::CommandType commandType;
    Type type;
};

class Hotkeys {
public:
    Hotkeys() = default;
    Hotkeys(const Hotkeys& rhs) = delete;
    Hotkeys& operator=(const Hotkeys& rhs) = delete;
    Hotkeys(Hotkeys&& rhs) = delete;
    Hotkeys& operator=(Hotkeys&& rhs) = delete;
    ~Hotkeys() = default;

    // Add a new hotkey
    void add(int keyPress, Type type, const ui::CommandType& commandType);
    // Get all hotkeys as a vector<int>
    std::vector<int> get(Type type);
    // Convert hotkey to ui::Command object
    std::optional<ui::Command> getCommandFromKeyPress(int keyPress);

private:
    std::flat_map<int, Kt> m_hotkeys;
};

} // namespace hotkeys