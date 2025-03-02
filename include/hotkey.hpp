#include <array>
#include <string>

enum HotkeyFlag {
    Hotkey_Flag_Alt = (1 << 0),
    Hotkey_Flag_LAlt = (1 << 1),
    Hotkey_Flag_RAlt = (1 << 2),

    Hotkey_Flag_Shift = (1 << 3),
    Hotkey_Flag_LShift = (1 << 4),
    Hotkey_Flag_RShift = (1 << 5),

    Hotkey_Flag_Cmd = (1 << 6),
    Hotkey_Flag_LCmd = (1 << 7),
    Hotkey_Flag_RCmd = (1 << 8),

    Hotkey_Flag_Control = (1 << 9),
    Hotkey_Flag_LControl = (1 << 10),
    Hotkey_Flag_RControl = (1 << 11),
    Hotkey_Flag_Fn = (1 << 12),

    Hotkey_Flag_Passthrough = (1 << 13),
};

inline const int l_offset = 1;
inline const int r_offset = 2;

inline const std::array<HotkeyFlag, 13> hotkey_flags = {
    Hotkey_Flag_Alt,
    Hotkey_Flag_LAlt,
    Hotkey_Flag_RAlt,

    Hotkey_Flag_Shift,
    Hotkey_Flag_LShift,
    Hotkey_Flag_RShift,

    Hotkey_Flag_Cmd,
    Hotkey_Flag_LCmd,
    Hotkey_Flag_RCmd,

    Hotkey_Flag_Control,
    Hotkey_Flag_LControl,
    Hotkey_Flag_RControl,

    Hotkey_Flag_Fn,
};

inline const std::array<std::string, 13> hotkey_flag_names = {
    "alt",
    "lalt",
    "ralt",

    "shift",
    "lshift",
    "rshift",

    "cmd",
    "lcmd",
    "rcmd",

    "ctrl",
    "lctrl",
    "rctrl",

    "fn",
};

inline const int alt_mod = 0;
inline const int shift_mod = 3;
inline const int cmd_mod = 6;
inline const int ctrl_mod = 9;
inline const int fn_mod = 12;

enum class KeyEventType {
    Down,  // default
    Up,
    Both,
};

struct CustomModifier {
    std::string name;
    int flags;  // Combined flags of constituent modifiers
};

struct Hotkey {
    int flags{};
    uint32_t keyCode{};
    std::string command;
    KeyEventType eventType{KeyEventType::Down};

    auto operator<=>(const Hotkey& other) const {
        if (auto cmp = flags <=> other.flags; cmp != 0) return cmp;
        if (auto cmp = keyCode <=> other.keyCode; cmp != 0) return cmp;
        return static_cast<int>(eventType) <=> static_cast<int>(other.eventType);
    }
};

bool compare_lr_mod(const Hotkey& a, const Hotkey& b, int mod);
bool compare_fn(const Hotkey& a, const Hotkey& b);
bool compare_key(const Hotkey& a, const Hotkey& b);
bool same_hotkey(const Hotkey& a, const Hotkey& b);
