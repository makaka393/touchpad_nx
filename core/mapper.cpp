#include "mapper.hpp"

#include <cmath>

namespace tp {

namespace {

// tan(22.5 deg): an axis counts as pressed while the thumb is within 67.5 deg of it,
// which splits the d-pad into eight equal sectors.
constexpr float kDiagonalRatio = 0.41421356f;

bool insideCircle(int x, int y, int cx, int cy, int radius) {
    const int dx = x - cx;
    const int dy = y - cy;
    return dx * dx + dy * dy <= radius * radius;
}

int16_t toAxis(float fraction) {
    const long v = std::lround(fraction * kStickMax);
    if (v > kStickMax) return kStickMax;
    if (v < -kStickMax) return -kStickMax;
    return static_cast<int16_t>(v);
}

}  // namespace

Mapper::Mapper(const Layout& layout, MapperConfig config) : m_layout(layout), m_config(config) {}

Visual Mapper::visual() const {
    Visual v;
    v.pad = m_pad;
    v.paused = m_paused;
    v.menuHold = m_menuHold;
    if (!m_paused) {
        v.touched = m_touched;
        v.grabL = m_grabL;
        v.grabR = m_grabR;
        v.lox = m_lox;
        v.loy = m_loy;
        v.rox = m_rox;
        v.roy = m_roy;
    }
    return v;
}

void Mapper::setPaused(bool paused) {
    if (paused == m_paused) return;
    m_paused = paused;
    // Whatever is on the glass right now must lift before it can act under the new state.
    for (int i = 0; i < m_fingerCount; i++) {
        if (m_fingers[i].role != Role::Menu) m_fingers[i].role = Role::Inert;
    }
    m_pad = PadState{};
    m_touched = 0;  // not a release: buttons that switch layouts must not fire
    m_grabL = m_grabR = false;
}

bool Mapper::takeMenuTap() {
    const bool tapped = m_menuTapped;
    m_menuTapped = false;
    return tapped;
}

int8_t Mapper::takeGoto() {
    const int8_t target = m_goto;
    m_goto = kGotoNone;
    return target;
}

void Mapper::reset() {
    for (int i = 0; i < m_fingerCount; i++) m_fingers[i].role = Role::Inert;
    m_pad = PadState{};
    m_menuHold = 0;
    m_touched = 0;
    m_latched = 0;
    m_goto = kGotoNone;
    m_grabL = m_grabR = false;
}

void Mapper::relayout() {
    for (int i = 0; i < m_fingerCount; i++) {
        Finger& f = m_fingers[i];
        const bool keeps = f.role == Role::Menu || (f.role == Role::StickL && m_layout.stickL.visible) ||
                           (f.role == Role::StickR && m_layout.stickR.visible);
        if (!keeps) f.role = Role::Inert;
    }
    m_pad.buttons = 0;
    m_touched = 0;
    m_latched = 0;
    m_goto = kGotoNone;
}

bool Mapper::onMenu(int x, int y) const {
    const MenuDef& m = m_layout.menu;
    return insideCircle(x, y, m.cx, m.cy, m.r + m_layout.slop);
}

uint32_t Mapper::buttonsAt(int x, int y) const {
    uint32_t hits = 0;
    for (int i = 0; i < m_layout.buttonCount; i++) {
        const ButtonDef& b = m_layout.buttons[i];
        if (!b.visible) continue;
        bool hit;
        if (b.shape == Shape::Circle) {
            hit = insideCircle(x, y, b.cx, b.cy, b.rx + m_layout.slop);
        } else {
            hit = std::abs(x - b.cx) <= b.rx + m_layout.slop && std::abs(y - b.cy) <= b.ry + m_layout.slop;
        }
        if (hit) hits |= 1u << i;
    }
    return hits;
}

bool Mapper::hasRole(Role role) const {
    for (int i = 0; i < m_fingerCount; i++) {
        if (m_fingers[i].role == role) return true;
    }
    return false;
}

bool Mapper::inStickZone(const StickDef& def, int x, int y) const {
    if (!def.visible) return false;
    if (def.floating) return x >= def.zx0 && x < def.zx1 && y >= def.zy0 && y < def.zy1;
    return insideCircle(x, y, def.cx, def.cy, def.zoneRadius);
}

Mapper::Role Mapper::classify(int x, int y, int8_t& anchor) const {
    anchor = -1;
    if (onMenu(x, y)) return Role::Menu;
    if (m_paused) return Role::Inert;

    const uint32_t hits = buttonsAt(x, y);
    if (hits) {
        // A drag button keeps its finger: from here on that finger steers, it does not slide
        // over to the neighbours.
        for (int i = 0; i < m_layout.buttonCount; i++) {
            if ((hits & (1u << i)) && m_layout.buttons[i].drag != Drag::None) {
                anchor = static_cast<int8_t>(i);
                return Role::Anchored;
            }
        }
        return Role::Free;
    }

    const DpadDef& d = m_layout.dpad;
    if (d.visible && insideCircle(x, y, d.cx, d.cy, d.zoneRadius) && !hasRole(Role::Dpad)) return Role::Dpad;
    if (inStickZone(m_layout.stickL, x, y) && !hasRole(Role::StickL)) return Role::StickL;
    if (inStickZone(m_layout.stickR, x, y) && !hasRole(Role::StickR)) return Role::StickR;
    return Role::Free;
}

void Mapper::stickFrom(Finger& f, float reach, bool follow, int x, int y, int16_t& outX, int16_t& outY) const {
    float dx = x - f.originX;
    float dy = y - f.originY;
    const float dist = std::sqrt(dx * dx + dy * dy);

    if (dist > reach) {
        const float k = reach / dist;
        if (follow) {
            // Drag the neutral point along so reversing direction responds immediately.
            f.originX = x - dx * k;
            f.originY = y - dy * k;
        }
        dx *= k;
        dy *= k;
    }

    if (dist < m_config.stickDeadzone * reach) {
        outX = 0;
        outY = 0;
        return;
    }
    outX = toAxis(dx / reach);
    outY = toAxis(-dy / reach);
}

uint32_t Mapper::dpadFrom(int x, int y) const {
    const DpadDef& d = m_layout.dpad;
    if (insideCircle(x, y, d.cx, d.cy, d.deadRadius)) return 0;

    const float dx = x - d.cx;
    const float dy = y - d.cy;
    const float ax = std::fabs(dx);
    const float ay = std::fabs(dy);

    uint32_t pressed = 0;
    if (ax > ay * kDiagonalRatio) pressed |= dx > 0 ? BtnRight : BtnLeft;
    if (ay > ax * kDiagonalRatio) pressed |= dy > 0 ? BtnDown : BtnUp;
    return pressed;
}

void Mapper::update(const TouchPoint* touches, int count, uint64_t nowMs) {
    for (int i = 0; i < m_fingerCount; i++) m_fingers[i].seen = false;

    PadState pad;
    uint8_t menuHold = 0;
    uint32_t touched = 0;
    bool grabL = false, grabR = false;

    for (int t = 0; t < count; t++) {
        const TouchPoint& p = touches[t];

        Finger* f = nullptr;
        for (int i = 0; i < m_fingerCount; i++) {
            if (m_fingers[i].id == p.id) {
                f = &m_fingers[i];
                break;
            }
        }
        if (!f) {
            if (m_fingerCount == kMaxFingers) continue;
            f = &m_fingers[m_fingerCount];
            int8_t anchor = -1;
            const Role role = classify(p.x, p.y, anchor);
            *f = Finger{p.id, role, static_cast<float>(p.x), static_cast<float>(p.y), nowMs, true, anchor, p.x, p.y};
            m_fingerCount++;
        }
        f->seen = true;
        f->x = p.x;
        f->y = p.y;

        switch (f->role) {
        case Role::Free:
            touched |= buttonsAt(p.x, p.y);
            break;
        case Role::Anchored:
            touched |= 1u << f->anchor;
            break;
        case Role::StickL:
            // A floating stick is drawn where it is: it stays put instead of chasing the thumb.
            stickFrom(*f, m_layout.stickL.throwRadius, !m_layout.stickL.floating, p.x, p.y, pad.lx, pad.ly);
            grabL = true;
            m_lox = static_cast<int16_t>(std::lround(f->originX));
            m_loy = static_cast<int16_t>(std::lround(f->originY));
            break;
        case Role::StickR:
            stickFrom(*f, m_layout.stickR.throwRadius, !m_layout.stickR.floating, p.x, p.y, pad.rx, pad.ry);
            grabR = true;
            m_rox = static_cast<int16_t>(std::lround(f->originX));
            m_roy = static_cast<int16_t>(std::lround(f->originY));
            break;
        case Role::Dpad:
            pad.buttons |= dpadFrom(p.x, p.y);
            break;
        case Role::Menu: {
            const MenuDef& m = m_layout.menu;
            if (!insideCircle(p.x, p.y, m.cx, m.cy, m.r + 2 * m_layout.slop)) {
                f->role = Role::Inert;  // slid away: neither a tap nor a hold
                break;
            }
            const uint64_t held = nowMs - f->downMs;
            if (held >= m_config.exitHoldMs) m_exitRequested = true;
            const uint64_t step = 1 + held * (kMenuHoldSteps - 1) / m_config.exitHoldMs;
            menuHold = static_cast<uint8_t>(step > kMenuHoldSteps ? kMenuHoldSteps : step);
            break;
        }
        case Role::Inert:
            break;
        }
    }

    // A finger holding a drag button steers its stick, unless a thumb is on the stick itself.
    for (int i = 0; i < m_fingerCount; i++) {
        Finger& f = m_fingers[i];
        if (!f.seen || f.role != Role::Anchored) continue;
        const Drag drag = m_layout.buttons[f.anchor].drag;
        const float reach = static_cast<float>(m_config.dragReach);
        if (drag == Drag::StickL && !grabL) stickFrom(f, reach, true, f.x, f.y, pad.lx, pad.ly);
        if (drag == Drag::StickR && !grabR) stickFrom(f, reach, true, f.x, f.y, pad.rx, pad.ry);
    }

    // Drop lifted fingers; a menu finger lifting before the exit threshold is a tap.
    int kept = 0;
    for (int i = 0; i < m_fingerCount; i++) {
        if (m_fingers[i].seen) {
            m_fingers[kept++] = m_fingers[i];
        } else if (m_fingers[i].role == Role::Menu && !m_exitRequested) {
            m_menuTapped = true;
        }
    }
    m_fingerCount = kept;

    const uint32_t rising = touched & ~m_touched;
    const uint32_t falling = m_touched & ~touched;
    for (int i = 0; i < m_layout.buttonCount; i++) {
        const ButtonDef& b = m_layout.buttons[i];
        const uint32_t bit = 1u << i;
        if (rising & bit) {
            m_downMs[i] = nowMs;
            if (b.press == Press::Toggle) m_latched ^= bit;
        }
        // Switching on release lets the game see the whole press first.
        if ((falling & bit) && b.gotoLayout != kGotoNone && m_goto == kGotoNone) m_goto = b.gotoLayout;

        bool down = false;
        switch (b.press) {
        case Press::Hold: down = touched & bit; break;
        case Press::Toggle: down = m_latched & bit; break;
        case Press::Turbo: down = (touched & bit) && ((nowMs - m_downMs[i]) / m_config.turboMs) % 2 == 0; break;
        }
        if (down) pad.buttons |= b.bit;
    }
    m_touched = touched;
    m_grabL = grabL;
    m_grabR = grabR;

    m_pad = m_paused ? PadState{} : pad;
    m_menuHold = menuHold;
}

}  // namespace tp
