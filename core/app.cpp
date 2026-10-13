#include "app.hpp"

#include <cstdio>

#include "text.hpp"

namespace tp {

namespace ui {

const char* rowLabel(int row) {
    switch (row) {
    case RowHide: return "Hide buttons";
    case RowFullPad: return "Full gamepad";
    case RowPack: return "Pack";
    case RowLayout: return "Layout";
    case RowEdit: return "Edit layout";
    case RowColor: return "Color";
    case RowOpacity: return "Brightness";
    case RowFaceColors: return "Colored ABXY";
    case RowUltrahand: return "Exit to Ultrahand";
    }
    return "";
}

const char* toolLabel(int tool, bool selectedIsShown) {
    switch (tool) {
    case ToolSmaller: return "−";
    case ToolBigger: return "+";
    case ToolShow: return selectedIsShown ? "Hide" : "Show";
    case ToolSave: return "Save";
    case ToolSaveNew: return "New";
    case ToolCancel: return "Cancel";
    }
    return "";
}

}  // namespace ui

namespace {

constexpr int kDragStart = 12;    // finger travel before a touch counts as a drag, not a tap
constexpr int kPressSlack = 14;   // how far a finger may stray from a row or tool and still count
constexpr int kGrabMargin = 10;

constexpr int kMinCircle = 24, kMaxCircle = 120, kCircleStep = 4;
constexpr int kMinRectW = 40, kMaxRectW = 220, kRectStepW = 6;
constexpr int kMinRectH = 26, kMaxRectH = 120, kRectStepH = 4;
constexpr int kMinStick = 60, kMaxStick = 150, kMinDpad = 60, kMaxDpad = 150, kBigStep = 6;

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

bool inside(const Rect& r, int x, int y, int slack = 0) {
    return x >= r.x0 - slack && x < r.x1 + slack && y >= r.y0 - slack && y < r.y1 + slack;
}

int dist2(int ax, int ay, int bx, int by) { return (ax - bx) * (ax - bx) + (ay - by) * (ay - by); }

}  // namespace

App::App() : m_layout(defaultLayout()), m_editBackup(defaultLayout()), m_mapper(m_layout) {
    addBuiltinPacks(m_packs);
    m_layout = m_packs.items[0].layouts[0].layout;
}

void App::setMode(Mode mode) {
    m_mode = mode;
    m_mapper.setPaused(mode != Mode::Play);
    m_finger = UiFinger{};
    m_revision++;
}

void App::applyPack(int index) {
    m_current = index;
    m_backCount = 0;
    m_fullPad = false;  // a pack picked by hand, or by a new game, is shown as it is
    m_held = false;
    applyLayout(m_packs.items[index].start, false);
    followGameState();
}

void App::applyLayout(int index, bool keepSticks) {
    m_layoutIndex = index;
    if (m_fullPad) {
        // The player's own arrangement of the full pad if there is one, the factory one otherwise.
        const int full = m_packs.find("full");
        m_layout = full >= 0 ? m_packs.items[full].layouts[0].layout : defaultLayout();
    } else {
        m_layout = m_packs.items[m_current].layouts[index].layout;
    }
    if (keepSticks) m_mapper.relayout();
    else m_mapper.reset();
    m_revision++;
}

void App::followGoto(int8_t target) {
    const Pack& pack = m_packs.items[m_current];
    int next;
    if (target == kGotoBack) {
        next = m_backCount > 0 ? m_back[--m_backCount] : pack.start;
        m_held = false;
        // While the game reports its state, it decides between the layouts that have rules.
        if (m_stateKnown && automatic(next)) {
            const int called = pack.layoutFor(m_stateBits);
            if (called >= 0) next = called;
        }
    } else {
        if (target >= 0 && target < pack.layoutCount && m_stateKnown && automatic(target)) {
            if (!pack.layouts[target].holds) return;
            m_held = true;
        }
        if (target < 0 || target >= pack.layoutCount || target == m_layoutIndex) return;
        if (m_backCount == kBackDepth) {
            for (int i = 1; i < kBackDepth; i++) m_back[i - 1] = m_back[i];
            m_backCount--;
        }
        m_back[m_backCount++] = static_cast<int8_t>(m_layoutIndex);
        next = target;
    }
    if (next == m_layoutIndex) return;
    // The thumb that is steering stays in charge across the switch.
    applyLayout(next, true);
}

void App::adoptLegacySettings() {
    if (!m_settings.legacy) return;
    for (int i = m_settings.gameCount - 1; i >= 0; i--) {
        const uint64_t game = m_settings.games[i].titleId;
        if (m_packs.findForGame(game) >= 0) m_settings.unbind(game);
    }
    m_settings.legacy = false;
    m_settingsDirty = true;
}

void App::usePack(int packIndex, int layoutIndex) {
    if (packIndex < 0 || packIndex >= m_packs.count) return;
    applyPack(packIndex);
    if (layoutIndex >= 0 && layoutIndex < m_packs.items[packIndex].layoutCount) applyLayout(layoutIndex, false);
}

void App::setGameState(bool known, uint32_t bits) {
    if (known == m_stateKnown && (!known || bits == m_stateBits)) return;
    m_stateKnown = known;
    m_stateBits = bits;
    followGameState();
}

void App::followGameState() {
    if (!m_stateKnown || m_fullPad || m_mode == Mode::Edit) return;  // never under the editor or the stand-in pad
    const int called = m_packs.items[m_current].layoutFor(m_stateBits);
    if (called < 0 || called == m_layoutIndex) return;
    if (automatic(m_layoutIndex) && !m_held) {
        applyLayout(called, true);
        return;
    }
    // A layout opened by hand stays up; "back" from it will land where the game is now.
    for (int i = 0; i < m_backCount; i++) {
        if (automatic(m_back[i])) m_back[i] = static_cast<int8_t>(called);
    }
}

void App::setGame(uint64_t titleId) {
    m_titleId = titleId;
    if (m_mode == Mode::Edit) return;  // never swap the layout out from under the editor

    const char* chosen = m_settings.packFor(titleId);
    int index = chosen ? m_packs.find(chosen) : -1;
    if (index < 0) index = m_packs.findForGame(titleId);
    if (index < 0) index = m_packs.find(m_settings.defaultPack);
    if (index < 0) index = 0;
    applyPack(index);
}

bool App::isNewTouch(uint32_t id) const {
    for (int i = 0; i < m_prevCount; i++) {
        if (m_prevIds[i] == id) return false;
    }
    return true;
}

const TouchPoint* App::findTouch(const TouchPoint* touches, int count, uint32_t id) const {
    for (int i = 0; i < count; i++) {
        if (touches[i].id == id) return &touches[i];
    }
    return nullptr;
}

void App::update(const TouchPoint* touches, int count, uint64_t nowMs) {
    m_mapper.update(touches, count, nowMs);
    if (m_mapper.exitRequested()) m_exit = true;
    const bool menuTap = m_mapper.takeMenuTap();
    const int8_t target = m_mapper.takeGoto();
    if (target != kGotoNone && m_mode == Mode::Play && !menuTap) followGoto(target);

    switch (m_mode) {
    case Mode::Play:
        if (menuTap) setMode(Mode::Menu);
        break;
    case Mode::Hidden:
        if (menuTap) setMode(Mode::Play);
        break;
    case Mode::Menu:
        if (menuTap) setMode(Mode::Play);
        else updateMenu(touches, count);
        break;
    case Mode::Edit:
        if (menuTap) cancelEdit();
        else updateEdit(touches, count);
        break;
    }

    m_prevCount = count > 16 ? 16 : count;
    for (int i = 0; i < m_prevCount; i++) m_prevIds[i] = touches[i].id;
}

// ---- menu ----

void App::updateMenu(const TouchPoint* touches, int count) {
    if (!m_finger.down) {
        for (int i = 0; i < count; i++) {
            const TouchPoint& t = touches[i];
            if (!isNewTouch(t.id)) continue;  // fingers from before the menu opened stay inert
            const MenuDef& m = m_layout.menu;
            if (dist2(t.x, t.y, m.cx, m.cy) <= (m.r + m_layout.slop) * (m.r + m_layout.slop)) continue;

            m_finger = UiFinger{};
            m_finger.down = true;
            m_finger.id = t.id;
            m_finger.outside = true;
            for (int row = 0; row < kMenuRows; row++) {
                if (!inside(ui::menuRow(row), t.x, t.y)) continue;
                m_finger.outside = false;
                m_finger.target = row;
                if (ui::rowHasArrows(row)) {
                    if (inside(ui::menuArrow(row, false), t.x, t.y)) m_finger.arrow = -1;
                    if (inside(ui::menuArrow(row, true), t.x, t.y)) m_finger.arrow = 1;
                }
            }
            if (inside(ui::kMenuPanel, t.x, t.y)) m_finger.outside = false;  // panel padding is not "outside"
            m_revision++;
            break;
        }
        return;
    }

    const TouchPoint* t = findTouch(touches, count, m_finger.id);
    if (t) {
        if (m_finger.target >= 0) {
            const Rect area = m_finger.arrow ? ui::menuArrow(m_finger.target, m_finger.arrow > 0) : ui::menuRow(m_finger.target);
            if (!inside(area, t->x, t->y, kPressSlack)) {
                m_finger.target = -1;  // slid away: no action on release
                m_revision++;
            }
        }
        return;
    }

    // Lifted.
    const UiFinger f = m_finger;
    m_finger = UiFinger{};
    m_revision++;
    if (f.target >= 0) activateRow(f.target, f.arrow);
    else if (f.outside) setMode(Mode::Play);
}

void App::activateRow(int row, int arrow) {
    Theme& theme = m_settings.theme;
    switch (row) {
    case RowHide:
        setMode(Mode::Hidden);
        break;
    case RowPack: {
        const int n = m_packs.count;
        applyPack((m_current + (arrow < 0 ? -1 : 1) + n) % n);
        // Remember the choice: for the running game, or as the general default without one.
        if (m_titleId) m_settings.bind(m_titleId, m_packs.items[m_current].id);
        else copyText(m_settings.defaultPack, sizeof(m_settings.defaultPack), m_packs.items[m_current].id);
        m_settingsDirty = true;
        break;
    }
    case RowFullPad:
        m_fullPad = !m_fullPad;
        applyLayout(m_layoutIndex, false);
        followGameState();
        break;
    case RowLayout: {
        const int n = m_packs.items[m_current].layoutCount;
        m_backCount = 0;
        m_held = false;
        m_fullPad = false;
        applyLayout((m_layoutIndex + (arrow < 0 ? -1 : 1) + n) % n, false);
        break;
    }
    case RowEdit:
        // What gets edited and saved is the pack's own layout, never the stand-in.
        if (m_fullPad) {
            m_fullPad = false;
            applyLayout(m_layoutIndex, false);
        }
        m_editBackup = m_layout;
        m_selected = ControlRef{};
        setMode(Mode::Edit);
        break;
    case RowColor: {
        const int at = presetIndex(theme);
        const int next = at < 0 ? (arrow < 0 ? kColorPresetCount - 1 : 0) : (at + (arrow < 0 ? -1 : 1) + kColorPresetCount) % kColorPresetCount;
        theme.r = kColorPresets[next].r;
        theme.g = kColorPresets[next].g;
        theme.b = kColorPresets[next].b;
        m_settingsDirty = true;
        break;
    }
    case RowOpacity: {
        int level = theme.opacity + (arrow < 0 ? -1 : 1);
        if (arrow == 0 && level > kMaxOpacity) level = kMinOpacity;  // tapping the row itself cycles
        theme.opacity = static_cast<uint8_t>(clampi(level, kMinOpacity, kMaxOpacity));
        m_settingsDirty = true;
        break;
    }
    case RowFaceColors:
        theme.faceColors = !theme.faceColors;
        m_settingsDirty = true;
        break;
    case RowUltrahand:
        m_exit = true;
        break;
    }
    m_revision++;
}

// ---- layout editor ----

ControlRef App::controlAt(int x, int y) const {
    ControlRef best;
    int bestDist = 1 << 30;
    auto consider = [&](ControlRef ref, int cx, int cy, int hw, int hh, bool round) {
        const bool hit = round ? dist2(x, y, cx, cy) <= (hw + kGrabMargin) * (hw + kGrabMargin)
                               : (x >= cx - hw - kGrabMargin && x <= cx + hw + kGrabMargin && y >= cy - hh - kGrabMargin && y <= cy + hh + kGrabMargin);
        const int d = dist2(x, y, cx, cy);
        if (hit && d < bestDist) {
            best = ref;
            bestDist = d;
        }
    };
    for (int i = 0; i < m_layout.buttonCount; i++) {
        const ButtonDef& b = m_layout.buttons[i];
        consider(ControlRef{ControlRef::ButtonC, static_cast<uint8_t>(i)}, b.cx, b.cy, b.rx, b.ry, b.shape == Shape::Circle);
    }
    consider(ControlRef{ControlRef::StickL, 0}, m_layout.stickL.cx, m_layout.stickL.cy, m_layout.stickL.baseRadius, 0, true);
    consider(ControlRef{ControlRef::StickR, 0}, m_layout.stickR.cx, m_layout.stickR.cy, m_layout.stickR.baseRadius, 0, true);
    consider(ControlRef{ControlRef::Dpad, 0}, m_layout.dpad.cx, m_layout.dpad.cy, m_layout.dpad.armLen, 0, true);
    return best;
}

void App::moveControl(ControlRef ref, int cx, int cy) {
    auto place = [&](int16_t& outX, int16_t& outY, int hw, int hh) {
        outX = static_cast<int16_t>(clampi(cx, hw, kScreenW - hw));
        outY = static_cast<int16_t>(clampi(cy, hh, kScreenH - hh));
    };
    switch (ref.kind) {
    case ControlRef::ButtonC: {
        ButtonDef& b = m_layout.buttons[ref.index];
        place(b.cx, b.cy, b.rx, b.ry);
        break;
    }
    case ControlRef::StickL: place(m_layout.stickL.cx, m_layout.stickL.cy, m_layout.stickL.baseRadius, m_layout.stickL.baseRadius); break;
    case ControlRef::StickR: place(m_layout.stickR.cx, m_layout.stickR.cy, m_layout.stickR.baseRadius, m_layout.stickR.baseRadius); break;
    case ControlRef::Dpad: place(m_layout.dpad.cx, m_layout.dpad.cy, m_layout.dpad.armLen, m_layout.dpad.armLen); break;
    case ControlRef::None: break;
    }
}

void App::resizeControl(ControlRef ref, int dir) {
    switch (ref.kind) {
    case ControlRef::ButtonC: {
        ButtonDef& b = m_layout.buttons[ref.index];
        if (b.shape == Shape::Circle) {
            b.rx = b.ry = static_cast<int16_t>(clampi(b.rx + dir * kCircleStep, kMinCircle, kMaxCircle));
        } else {
            b.rx = static_cast<int16_t>(clampi(b.rx + dir * kRectStepW, kMinRectW, kMaxRectW));
            b.ry = static_cast<int16_t>(clampi(b.ry + dir * kRectStepH, kMinRectH, kMaxRectH));
        }
        moveControl(ref, b.cx, b.cy);  // a grown control may need nudging back on screen
        break;
    }
    case ControlRef::StickL:
    case ControlRef::StickR: {
        StickDef& s = ref.kind == ControlRef::StickL ? m_layout.stickL : m_layout.stickR;
        setStickSize(s, clampi(s.baseRadius + dir * kBigStep, kMinStick, kMaxStick));
        moveControl(ref, s.cx, s.cy);
        break;
    }
    case ControlRef::Dpad:
        setDpadSize(m_layout.dpad, clampi(m_layout.dpad.armLen + dir * kBigStep, kMinDpad, kMaxDpad));
        moveControl(ref, m_layout.dpad.cx, m_layout.dpad.cy);
        break;
    case ControlRef::None: break;
    }
}

bool App::isShown(ControlRef ref) const {
    switch (ref.kind) {
    case ControlRef::ButtonC: return m_layout.buttons[ref.index].visible;
    case ControlRef::StickL: return m_layout.stickL.visible;
    case ControlRef::StickR: return m_layout.stickR.visible;
    case ControlRef::Dpad: return m_layout.dpad.visible;
    case ControlRef::None: break;
    }
    return true;
}

void App::setShown(ControlRef ref, bool shown) {
    switch (ref.kind) {
    case ControlRef::ButtonC: m_layout.buttons[ref.index].visible = shown; break;
    case ControlRef::StickL: m_layout.stickL.visible = shown; break;
    case ControlRef::StickR: m_layout.stickR.visible = shown; break;
    case ControlRef::Dpad: m_layout.dpad.visible = shown; break;
    case ControlRef::None: break;
    }
}

void App::updateEdit(const TouchPoint* touches, int count) {
    if (!m_finger.down) {
        for (int i = 0; i < count; i++) {
            const TouchPoint& t = touches[i];
            if (!isNewTouch(t.id)) continue;
            const MenuDef& m = m_layout.menu;
            if (dist2(t.x, t.y, m.cx, m.cy) <= (m.r + m_layout.slop) * (m.r + m_layout.slop)) continue;

            m_finger = UiFinger{};
            m_finger.down = true;
            m_finger.id = t.id;
            m_finger.startX = t.x;
            m_finger.startY = t.y;
            if (inside(ui::kToolbar, t.x, t.y)) {
                // The toolbar swallows touches so nothing underneath gets dragged by accident.
                for (int tool = 0; tool < kEditTools; tool++) {
                    if (inside(ui::toolButton(tool), t.x, t.y)) m_finger.target = tool;
                }
            } else {
                m_finger.control = controlAt(t.x, t.y);
                m_finger.outside = m_finger.control.kind == ControlRef::None;
                if (m_finger.control.kind == ControlRef::ButtonC) {
                    m_finger.grabDx = m_layout.buttons[m_finger.control.index].cx - t.x;
                    m_finger.grabDy = m_layout.buttons[m_finger.control.index].cy - t.y;
                } else if (m_finger.control.kind == ControlRef::StickL || m_finger.control.kind == ControlRef::StickR) {
                    const StickDef& s = m_finger.control.kind == ControlRef::StickL ? m_layout.stickL : m_layout.stickR;
                    m_finger.grabDx = s.cx - t.x;
                    m_finger.grabDy = s.cy - t.y;
                } else if (m_finger.control.kind == ControlRef::Dpad) {
                    m_finger.grabDx = m_layout.dpad.cx - t.x;
                    m_finger.grabDy = m_layout.dpad.cy - t.y;
                }
            }
            m_revision++;
            break;
        }
        return;
    }

    const TouchPoint* t = findTouch(touches, count, m_finger.id);
    if (t) {
        if (m_finger.target >= 0) {
            if (!inside(ui::toolButton(m_finger.target), t->x, t->y, kPressSlack)) {
                m_finger.target = -1;
                m_revision++;
            }
        } else if (m_finger.control.kind != ControlRef::None) {
            if (!m_finger.dragging && dist2(t->x, t->y, m_finger.startX, m_finger.startY) > kDragStart * kDragStart) {
                m_finger.dragging = true;
                m_selected = m_finger.control;
            }
            if (m_finger.dragging) {
                moveControl(m_finger.control, t->x + m_finger.grabDx, t->y + m_finger.grabDy);
                m_revision++;
            }
        }
        return;
    }

    // Lifted.
    const UiFinger f = m_finger;
    m_finger = UiFinger{};
    m_revision++;
    if (f.target >= 0) activateTool(f.target);
    else if (f.control.kind != ControlRef::None && !f.dragging) m_selected = f.control;
    else if (f.outside) m_selected = ControlRef{};
}

void App::activateTool(int tool) {
    const bool hasSelection = m_selected.kind != ControlRef::None;
    switch (tool) {
    case ToolSmaller:
        if (hasSelection) resizeControl(m_selected, -1);
        break;
    case ToolBigger:
        if (hasSelection) resizeControl(m_selected, 1);
        break;
    case ToolShow:
        if (hasSelection) setShown(m_selected, !isShown(m_selected));
        break;
    case ToolSave:
        finishEdit(false);
        break;
    case ToolSaveNew:
        finishEdit(true);
        break;
    case ToolCancel:
        cancelEdit();
        break;
    }
    m_revision++;
}

void App::finishEdit(bool asNew) {
    int index = m_current;
    if (asNew && m_packs.count < kMaxPacks) {
        // A copy of the whole pack under the first free "customN" name, built in place: a
        // pack is too big to pass around on the input thread's stack.
        char id[sizeof(Pack::id)] = "";
        unsigned number = 0;
        for (int n = 1; n <= kMaxPacks + 1; n++) {
            number = static_cast<unsigned>(n) % 100u;
            std::snprintf(id, sizeof(id), "custom%u", number);
            if (m_packs.find(id) < 0) break;
        }
        Pack& fresh = m_packs.items[m_packs.count];
        fresh = m_packs.items[m_current];
        copyText(fresh.id, sizeof(fresh.id), id);
        std::snprintf(fresh.name, sizeof(fresh.name), "Custom %u", number);
        fresh.gameCount = 0;  // it is chosen by hand, the original stays the game's own pack
        index = m_packs.count++;
        if (m_titleId) m_settings.bind(m_titleId, fresh.id);
        else copyText(m_settings.defaultPack, sizeof(m_settings.defaultPack), fresh.id);
        m_settingsDirty = true;
    }
    m_packs.items[index].layouts[m_layoutIndex].layout = m_layout;
    m_current = index;
    m_packToSave = index;
    m_selected = ControlRef{};
    m_mapper.reset();
    setMode(Mode::Play);
    followGameState();  // the game may have moved on while the editor was up
}

void App::cancelEdit() {
    m_layout = m_editBackup;
    m_selected = ControlRef{};
    m_mapper.reset();
    setMode(Mode::Play);
    followGameState();
}

// ---- output ----

Visual App::visual() const {
    Visual v = m_mapper.visual();
    v.paused = m_mode == Mode::Hidden;
    return v;
}

void App::fillScene(Scene& out) const {
    out.mode = m_mode;
    out.visual = visual();
    out.revision = m_revision;
    out.layout = m_layout;
    out.theme = m_settings.theme;
    const Pack& pack = m_packs.items[m_current];
    copyText(out.packName, sizeof(out.packName), pack.name);
    copyText(out.layoutName, sizeof(out.layoutName), m_fullPad ? ui::rowLabel(RowFullPad) : pack.layouts[m_layoutIndex].name);

    for (int row = 0; row < kMenuRows; row++) out.rowValue[row][0] = 0;
    copyText(out.rowValue[RowPack], sizeof(out.rowValue[RowPack]), pack.name);
    copyText(out.rowValue[RowLayout], sizeof(out.rowValue[RowLayout]), pack.layouts[m_layoutIndex].name);
    copyText(out.rowValue[RowFullPad], sizeof(out.rowValue[RowFullPad]), m_fullPad ? "on" : "off");
    const int preset = presetIndex(m_settings.theme);
    copyText(out.rowValue[RowColor], sizeof(out.rowValue[RowColor]), preset >= 0 ? kColorPresets[preset].name : "Custom");
    std::snprintf(out.rowValue[RowOpacity], sizeof(out.rowValue[RowOpacity]), "%u / %u", m_settings.theme.opacity % 10u,
                  static_cast<unsigned>(kMaxOpacity));
    copyText(out.rowValue[RowFaceColors], sizeof(out.rowValue[RowFaceColors]), m_settings.theme.faceColors ? "on" : "off");

    const bool inMenu = m_mode == Mode::Menu && m_finger.down;
    out.pressedRow = static_cast<int8_t>(inMenu ? m_finger.target : -1);
    out.pressedArrow = static_cast<int8_t>(inMenu && m_finger.target >= 0 ? m_finger.arrow : 0);
    out.selected = m_mode == Mode::Edit ? m_selected : ControlRef{};
    out.pressedTool = static_cast<int8_t>(m_mode == Mode::Edit && m_finger.down ? m_finger.target : -1);
}

bool App::takeExitRequest() {
    const bool exit = m_exit;
    m_exit = false;
    return exit;
}

bool App::takeSettingsDirty() {
    const bool dirty = m_settingsDirty;
    m_settingsDirty = false;
    return dirty;
}

int App::takePackToSave() {
    const int index = m_packToSave;
    m_packToSave = -1;
    return index;
}

}  // namespace tp
