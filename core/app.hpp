// Everything the overlay does with touches: playing, the hidden state, the settings menu and
// the layout editor. Owns the packs and settings; the platform loads and saves them.
#pragma once
#include "canvas.hpp"
#include "config.hpp"
#include "mapper.hpp"

namespace tp {

enum class Mode : uint8_t { Play, Hidden, Menu, Edit };

enum MenuRow : int { RowHide, RowFullPad, RowPack, RowLayout, RowEdit, RowColor, RowOpacity, RowFaceColors, RowUltrahand, kMenuRows };
enum EditTool : int { ToolSmaller, ToolBigger, ToolShow, ToolSave, ToolSaveNew, ToolCancel, kEditTools };

struct ControlRef {
    enum Kind : uint8_t { None, ButtonC, StickL, StickR, Dpad } kind = None;
    uint8_t index = 0;
    bool operator==(const ControlRef&) const = default;
};

// Placement of the menu and the editor toolbar, shared by hit testing and drawing.
namespace ui {

constexpr int kRowH = 56;
constexpr int kArrowW = 72;
constexpr Rect kMenuPanel{320, 96, 960, 96 + 16 + kMenuRows * kRowH + 16};
// Bottom centre: the one area a gamepad layout normally leaves empty.
constexpr Rect kToolbar{440, 476, 840, 672};
constexpr int kToolW[kEditTools] = {110, 110, 140, 150, 104, 106};  // two rows of three

constexpr Rect menuRow(int row) {
    return Rect{kMenuPanel.x0 + 16, kMenuPanel.y0 + 16 + row * kRowH, kMenuPanel.x1 - 16, kMenuPanel.y0 + 16 + (row + 1) * kRowH};
}
constexpr bool rowHasArrows(int row) { return row == RowPack || row == RowLayout || row == RowColor || row == RowOpacity; }
constexpr Rect menuArrow(int row, bool right) {
    const Rect r = menuRow(row);
    return right ? Rect{r.x1 - kArrowW, r.y0, r.x1, r.y1} : Rect{r.x1 - 300, r.y0, r.x1 - 300 + kArrowW, r.y1};
}
constexpr Rect toolButton(int tool) {
    const int row = tool / 3;
    int x = kToolbar.x0 + 12;
    for (int i = row * 3; i < tool; i++) x += kToolW[i] + 8;
    const int y = kToolbar.y0 + 66 + row * 64;
    return Rect{x, y, x + kToolW[tool], y + 56};
}

const char* rowLabel(int row);
const char* toolLabel(int tool, bool selectedIsShown);

}  // namespace ui

// Snapshot of what should be on screen; copied to the render thread.
struct Scene {
    Mode mode = Mode::Play;
    Visual visual;
    uint32_t revision = 0;  // changes with everything below; `visual` is compared on its own
    Layout layout;
    Theme theme;
    char packName[32] = "";
    char layoutName[32] = "";
    char rowValue[kMenuRows][32] = {};
    int8_t pressedRow = -1;   // menu row under the finger
    int8_t pressedArrow = 0;  // -1 left arrow, +1 right arrow, 0 the row itself
    ControlRef selected;      // editor selection
    int8_t pressedTool = -1;
};

class App {
public:
    App();

    PackStore& packs() { return m_packs; }
    Settings& settings() { return m_settings; }

    // Call once packs and settings are loaded and whenever the running game changes
    // (0 = none). Picks the pack chosen for that game before, else one made for it, else the
    // default pack.
    void setGame(uint64_t titleId);

    // Call after the settings and the packs are loaded. Settings written by 0.2 remember a
    // profile for games that have a pack of their own now; such a game gets its pack, and the
    // settings are flagged for rewriting so this happens once.
    void adoptLegacySettings();

    // Switches to a pack (and one of its layouts, or its starting one) without touching the
    // settings: for tools and tests.
    void usePack(int packIndex, int layoutIndex = -1);

    // What a mod inside the game reports about the player, as the bits the pack names in its
    // [state] section; `known` is false while there is no such report. With a report, layouts
    // that have a `when` rule come and go by themselves and buttons no longer switch to them.
    void setGameState(bool known, uint32_t bits);
    // Whether the pack in use has rules at all: without them nobody needs to look for a report.
    bool wantsGameState() const { return m_packs.items[m_current].followsGameState(); }
    bool gameStateKnown() const { return m_stateKnown; }

    void update(const TouchPoint* touches, int count, uint64_t nowMs);

    const PadState& pad() const { return m_mapper.pad(); }
    Mode mode() const { return m_mode; }
    // False while the pad is hidden, and during play on a layout that leaves touches to the game.
    bool blockGameTouch() const { return m_mode != Mode::Hidden && !(m_mode == Mode::Play && m_layout.touchToGame); }
    int currentPack() const { return m_current; }
    int currentLayout() const { return m_layoutIndex; }
    // The "full" pack (every button, wherever the player put them) is standing in for the
    // pack's layout: the way out when a pack turns out to be unusable in some spot of a game.
    bool fullPad() const { return m_fullPad; }
    uint32_t revision() const { return m_revision; }
    Visual visual() const;
    void fillScene(Scene& out) const;

    // Requests for the platform; each reports once.
    bool takeExitRequest();
    bool takeSettingsDirty();
    int takePackToSave();  // index into packs(), or -1

private:
    struct UiFinger {
        bool down = false;
        uint32_t id = 0;
        int startX = 0, startY = 0;
        int target = -1;       // menu: row; editor: tool
        int arrow = 0;         // menu only
        ControlRef control;    // editor only
        int grabDx = 0, grabDy = 0;
        bool dragging = false;
        bool outside = false;  // landed on nothing
    };

    void setMode(Mode mode);
    void applyPack(int index);
    void applyLayout(int index, bool keepSticks);
    void followGoto(int8_t target);
    void followGameState();
    bool automatic(int layout) const { return m_packs.items[m_current].layouts[layout].when.terms > 0; }
    void updateMenu(const TouchPoint* touches, int count);
    void updateEdit(const TouchPoint* touches, int count);
    void activateRow(int row, int arrow);
    void activateTool(int tool);
    void finishEdit(bool asNew);
    void cancelEdit();
    ControlRef controlAt(int x, int y) const;
    void moveControl(ControlRef ref, int cx, int cy);
    void resizeControl(ControlRef ref, int dir);
    bool isShown(ControlRef ref) const;
    void setShown(ControlRef ref, bool shown);
    bool isNewTouch(uint32_t id) const;
    const TouchPoint* findTouch(const TouchPoint* touches, int count, uint32_t id) const;

    PackStore m_packs;
    Settings m_settings;
    Layout m_layout;       // the layout in use; the mapper reads it directly
    Layout m_editBackup;
    Mapper m_mapper;
    Mode m_mode = Mode::Play;
    int m_current = 0;      // pack
    int m_layoutIndex = 0;  // layout within it
    bool m_fullPad = false; // the built-in full gamepad is shown instead of that layout
    bool m_stateKnown = false;
    bool m_held = false;    // the layout on screen was opened by a button and holds
    uint32_t m_stateBits = 0;
    static constexpr int kBackDepth = 8;
    int8_t m_back[kBackDepth] = {};  // layouts to return to, oldest first
    int m_backCount = 0;
    uint64_t m_titleId = 0;
    uint32_t m_revision = 1;
    UiFinger m_finger;
    ControlRef m_selected;
    uint32_t m_prevIds[16];
    int m_prevCount = 0;
    bool m_exit = false;
    bool m_settingsDirty = false;
    int m_packToSave = -1;
};

}  // namespace tp
