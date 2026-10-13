// On-screen control placement, in logical 1280x720 coordinates.
#pragma once
#include "types.hpp"

namespace tp {

enum class Shape : uint8_t { Circle, RoundRect };

// What is drawn inside a button: a controller legend or a pictogram for a game action.
enum class Glyph : uint8_t {
    None, A, B, X, Y, L, R, ZL, ZR, Plus, Minus, Home, Capture, StickClick,
    // Pictograms, one per line of assets/icons.txt.
#define ICON(id, name) id,
#include "icon_list.inc"
#undef ICON
};

constexpr int kFirstIcon = static_cast<int>(Glyph::StickClick) + 1;
constexpr int kIconCount = 0
#define ICON(id, name) +1
#include "icon_list.inc"
#undef ICON
    ;

// How a touch on a button turns into a press.
enum class Press : uint8_t {
    Hold,    // pressed while touched
    Toggle,  // each touch flips it; stays pressed in between
    Turbo,   // pressed and released repeatedly while touched
};

// A button that also steers a stick: the finger holding it deflects the stick by moving.
enum class Drag : uint8_t { None, StickL, StickR };

// ButtonDef::gotoLayout: index of a layout in the same pack, or one of these.
constexpr int8_t kGotoNone = -1;
constexpr int8_t kGotoBack = -2;  // the layout that was active before the current one

struct ButtonDef {
    uint32_t bit;  // pad buttons it presses: one bit, several for a combo, 0 for none
    Shape shape;
    int16_t cx, cy;
    int16_t rx, ry;  // half extents; a circle uses rx as its radius
    Glyph glyph;
    bool visible = true;  // a layout can leave a control out entirely
    Press press = Press::Hold;
    Drag drag = Drag::None;
    int8_t gotoLayout = kGotoNone;  // layout to switch to when the button is let go
    bool tinted = false;            // drawn in its own colour instead of the theme's
    uint8_t r = 15, g = 15, b = 15;
    char label[24] = "";            // UTF-8 caption; alone or under the glyph
    bool operator==(const ButtonDef&) const = default;
};

struct StickDef {
    int16_t cx, cy;
    int16_t baseRadius;   // drawn ring
    int16_t zoneRadius;   // a touch that lands inside grabs the stick
    int16_t throwRadius;  // finger travel for full deflection
    bool visible = true;
    // A floating stick has no fixed place: a touch anywhere in its zone rectangle grabs it
    // and becomes its centre. (cx, cy) is then only where it rests while untouched.
    bool floating = false;
    bool hint = true;     // floating only: draw it at rest
    int16_t zx0 = 0, zy0 = 0, zx1 = 0, zy1 = 0;
    bool operator==(const StickDef&) const = default;
};

struct DpadDef {
    int16_t cx, cy;
    int16_t armLen;        // centre to arm tip
    int16_t armHalfWidth;
    int16_t zoneRadius;
    int16_t deadRadius;
    bool visible = true;
    bool operator==(const DpadDef&) const = default;
};

struct MenuDef {
    int16_t cx, cy, r;
    bool operator==(const MenuDef&) const = default;
};

struct Layout {
    static constexpr int kMaxButtons = 24;
    ButtonDef buttons[kMaxButtons];
    int buttonCount;
    StickDef stickL, stickR;
    DpadDef dpad;
    MenuDef menu;
    int16_t slop;  // extra hit margin around buttons, forgiving imprecise thumbs
    // The game keeps receiving touches while this layout is up (menus that are touch driven).
    bool touchToGame = false;

    // Same controls in the same places; slots past buttonCount do not count.
    bool operator==(const Layout& o) const {
        if (buttonCount != o.buttonCount) return false;
        for (int i = 0; i < buttonCount; i++) {
            if (!(buttons[i] == o.buttons[i])) return false;
        }
        return stickL == o.stickL && stickR == o.stickR && dpad == o.dpad && menu == o.menu && slop == o.slop &&
               touchToGame == o.touchToGame;
    }
};

const Layout& defaultLayout();

// Sticks and the d-pad are sized by one number; the touch zones follow from it.
void setStickSize(StickDef& stick, int baseRadius);
void setDpadSize(DpadDef& dpad, int armLen);

}  // namespace tp
