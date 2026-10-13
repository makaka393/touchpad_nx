#include "render.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "icons.hpp"

namespace tp {

namespace {

constexpr float kOutline = 2.0f;
constexpr float kKnobRatio = 0.42f;  // knob radius relative to the stick base
constexpr int kPad = 4;              // room around a control for its outline and anti-aliasing
constexpr uint32_t kDpadBits = BtnLeft | BtnUp | BtnRight | BtnDown;

struct Seg { float x0, y0, x1, y1; };

// Letters as strokes in a box about 1 wide and 1.4 tall, centred on the origin, y down.
constexpr Seg kGlyphA[] = {{-0.45f, 0.7f, 0, -0.7f}, {0, -0.7f, 0.45f, 0.7f}, {-0.27f, 0.22f, 0.27f, 0.22f}};
constexpr Seg kGlyphB[] = {
    {-0.38f, -0.7f, -0.38f, 0.7f}, {-0.38f, -0.7f, 0.12f, -0.7f}, {0.12f, -0.7f, 0.36f, -0.52f},
    {0.36f, -0.52f, 0.36f, -0.18f}, {0.36f, -0.18f, 0.12f, 0}, {-0.38f, 0, 0.12f, 0},
    {0.12f, 0, 0.42f, 0.18f}, {0.42f, 0.18f, 0.42f, 0.52f}, {0.42f, 0.52f, 0.15f, 0.7f},
    {0.15f, 0.7f, -0.38f, 0.7f}};
constexpr Seg kGlyphX[] = {{-0.42f, -0.7f, 0.42f, 0.7f}, {0.42f, -0.7f, -0.42f, 0.7f}};
constexpr Seg kGlyphY[] = {{-0.45f, -0.7f, 0, 0}, {0.45f, -0.7f, 0, 0}, {0, 0, 0, 0.7f}};
constexpr Seg kGlyphL[] = {{-0.32f, -0.7f, -0.32f, 0.7f}, {-0.32f, 0.7f, 0.38f, 0.7f}};
constexpr Seg kGlyphR[] = {
    {-0.38f, 0.7f, -0.38f, -0.7f}, {-0.38f, -0.7f, 0.12f, -0.7f}, {0.12f, -0.7f, 0.38f, -0.52f},
    {0.38f, -0.52f, 0.38f, -0.18f}, {0.38f, -0.18f, 0.12f, 0}, {-0.38f, 0, 0.12f, 0},
    {0.05f, 0, 0.42f, 0.7f}};
constexpr Seg kGlyphZ[] = {{-0.38f, -0.7f, 0.38f, -0.7f}, {0.38f, -0.7f, -0.38f, 0.7f}, {-0.38f, 0.7f, 0.38f, 0.7f}};
constexpr Seg kGlyphPlus[] = {{-0.6f, 0, 0.6f, 0}, {0, -0.6f, 0, 0.6f}};
constexpr Seg kGlyphMinus[] = {{-0.6f, 0, 0.6f, 0}};
constexpr Seg kGlyphHome[] = {
    {-0.72f, 0.02f, 0, -0.68f}, {0, -0.68f, 0.72f, 0.02f}, {-0.46f, -0.1f, -0.46f, 0.64f},
    {0.46f, -0.1f, 0.46f, 0.64f}, {-0.46f, 0.64f, 0.46f, 0.64f}};
constexpr Seg kGlyphMenu[] = {{-0.6f, -0.45f, 0.6f, -0.45f}, {-0.6f, 0, 0.6f, 0}, {-0.6f, 0.45f, 0.6f, 0.45f}};

template <int N>
void strokes(Canvas& c, const Seg (&segs)[N], float cx, float cy, float size, Color col) {
    const float width = size * 0.21f;
    for (const Seg& s : segs) {
        c.line(cx + s.x0 * size, cy + s.y0 * size, cx + s.x1 * size, cy + s.y1 * size, width, col);
    }
}

void drawGlyph(Canvas& c, Glyph g, float cx, float cy, float size, Color col) {
    switch (g) {
    case Glyph::A: strokes(c, kGlyphA, cx, cy, size, col); break;
    case Glyph::B: strokes(c, kGlyphB, cx, cy, size, col); break;
    case Glyph::X: strokes(c, kGlyphX, cx, cy, size, col); break;
    case Glyph::Y: strokes(c, kGlyphY, cx, cy, size, col); break;
    case Glyph::L: strokes(c, kGlyphL, cx, cy, size, col); break;
    case Glyph::R: strokes(c, kGlyphR, cx, cy, size, col); break;
    case Glyph::ZL:
        strokes(c, kGlyphZ, cx - size * 0.58f, cy, size, col);
        strokes(c, kGlyphL, cx + size * 0.58f, cy, size, col);
        break;
    case Glyph::ZR:
        strokes(c, kGlyphZ, cx - size * 0.58f, cy, size, col);
        strokes(c, kGlyphR, cx + size * 0.58f, cy, size, col);
        break;
    case Glyph::Plus: strokes(c, kGlyphPlus, cx, cy, size, col); break;
    case Glyph::Minus: strokes(c, kGlyphMinus, cx, cy, size, col); break;
    case Glyph::Home: strokes(c, kGlyphHome, cx, cy, size, col); break;
    case Glyph::Capture:
        c.strokeRoundRect(cx, cy, size * 0.62f, size * 0.62f, size * 0.2f, size * 0.24f, col);
        c.fillCircle(cx, cy, size * 0.26f, col);
        break;
    case Glyph::StickClick:
        c.strokeCircle(cx, cy, size * 0.66f, size * 0.24f, col);
        c.fillCircle(cx, cy, size * 0.28f, col);
        break;
    default:
        drawIcon(c, g, cx, cy, size, col);
        break;
    }
}

Rect around(int cx, int cy, int hw, int hh) {
    return Rect{cx - hw - kPad, cy - hh - kPad, cx + hw + kPad, cy + hh + kPad};
}

Rect unite(const Rect& a, const Rect& b) {
    return Rect{a.x0 < b.x0 ? a.x0 : b.x0, a.y0 < b.y0 ? a.y0 : b.y0,
                a.x1 > b.x1 ? a.x1 : b.x1, a.y1 > b.y1 ? a.y1 : b.y1};
}

// Knob offset in whole framebuffer pixels, packed into a key (so sub-pixel wobble repaints nothing).
constexpr int kKnobBias = 512;
uint32_t packKnob(int kx, int ky) { return static_cast<uint32_t>(((kx + kKnobBias) << 12) | (ky + kKnobBias)); }
int knobX(uint32_t key) { return static_cast<int>((key >> 12) & 0xFFF) - kKnobBias; }
int knobY(uint32_t key) { return static_cast<int>(key & 0xFFF) - kKnobBias; }

constexpr Color kPanelFill{0, 0, 0, 13};
constexpr Color kTextColor{15, 15, 15, 15};
constexpr Color kSelection{15, 13, 2, 15};
constexpr float kLabelSize = 30.0f;
constexpr float kToolSize = 25.0f;
constexpr float kBaselineShift = 0.34f;  // from the vertical middle of a line down to its baseline
constexpr float kMinCaption = 17.0f;     // smaller than this is unreadable on the 640x360 layer

// SNES-style colours for the face buttons.
struct FaceColor { uint32_t bit; uint8_t r, g, b; };
constexpr FaceColor kFaceColors[] = {{BtnA, 15, 4, 4}, {BtnB, 15, 13, 2}, {BtnX, 4, 8, 15}, {BtnY, 4, 13, 6}};

Rect whole() { return Rect{0, 0, kScreenW, kScreenH}; }
float midX(const Rect& r) { return (r.x0 + r.x1) * 0.5f; }
float midY(const Rect& r) { return (r.y0 + r.y1) * 0.5f; }

}  // namespace

Renderer::Renderer(Canvas& canvas, const Font* font) : m_canvas(canvas), m_font(font) {}

Renderer::Style Renderer::styleFor(uint8_t r, uint8_t g, uint8_t b, bool ghost) const {
    // Alpha values are tuned for opacity level 4; other levels scale them, ghosts are a third.
    const float k = (m_scene.theme.opacity / 4.0f) * (ghost ? 0.34f : 1.0f);
    auto a = [k](int base) {
        const int v = static_cast<int>(base * k + 0.5f);
        return static_cast<uint8_t>(v < 1 ? 1 : (v > 15 ? 15 : v));
    };
    Style st;
    // Light glass: a faint wash of the accent colour, a thin brighter edge, a clear legend.
    st.fill = Color{r, g, b, a(3)};
    st.shadow = Color{0, 0, 0, a(2)};
    st.outline = kOutline;
    st.line = Color{r, g, b, a(6)};
    st.glyph = Color{r, g, b, a(13)};
    st.fillDown = Color{r, g, b, a(11)};
    st.lineDown = Color{r, g, b, a(15)};
    st.glyphDown = Color{0, 0, 0, a(12)};
    st.baseFill = Color{r, g, b, a(2)};
    st.baseLine = Color{r, g, b, a(5)};
    st.knobIdle = Color{r, g, b, a(7)};
    st.knobActive = Color{r, g, b, a(12)};
    return st;
}

Renderer::Style Renderer::styleForButton(const ButtonDef& b) const {
    const Theme& t = m_scene.theme;
    const bool ghost = !b.visible;  // only ever drawn in the editor
    if (b.tinted) {
        // Its own colour goes into a heavier ring (and fills the button while pressed); the
        // legend keeps the theme's colour, so every button reads the same way.
        Style st = styleFor(b.r, b.g, b.b, ghost);
        st.outline = kOutline + 1.0f;
        st.line.a = st.glyph.a;
        st.glyph = styleFor(t.r, t.g, t.b, ghost).glyph;
        return st;
    }
    if (t.faceColors) {
        for (const FaceColor& f : kFaceColors) {
            if (f.bit == b.bit) return styleFor(f.r, f.g, f.b, ghost);
        }
    }
    return styleFor(t.r, t.g, t.b, ghost);
}

void Renderer::rebuildElements() {
    const Layout& l = m_scene.layout;
    m_elementCount = 0;
    for (int i = 0; i < l.buttonCount; i++) {
        const ButtonDef& b = l.buttons[i];
        m_elements[m_elementCount++] = Element{Element::ButtonEl, static_cast<uint8_t>(i), around(b.cx, b.cy, b.rx, b.ry), 0};
    }
    const Element::Kind sticks[] = {Element::StickLEl, Element::StickREl};
    for (const Element::Kind kind : sticks) {
        Element e{kind, 0, Rect{}, 0};
        e.pose = poseFor(e, m_scene.visual);
        const int r = stickOf(e).baseRadius;
        e.bounds = around(e.pose.x, e.pose.y, r, r);
        m_elements[m_elementCount++] = e;
    }
    m_elements[m_elementCount++] = Element{Element::DpadEl, 0, around(l.dpad.cx, l.dpad.cy, l.dpad.armLen, l.dpad.armLen), 0};
    m_elements[m_elementCount++] = Element{Element::MenuEl, 0, around(l.menu.cx, l.menu.cy, l.menu.r, l.menu.r), 0};
}

const StickDef& Renderer::stickOf(const Element& e) const {
    return e.kind == Element::StickLEl ? m_scene.layout.stickL : m_scene.layout.stickR;
}

Renderer::StickPose Renderer::poseFor(const Element& e, const Visual& v) const {
    const StickDef& s = stickOf(e);
    const StickPose home{s.cx, s.cy, true};
    // The editor moves the resting place, and shows left-out sticks as ghosts.
    if (m_scene.mode == Mode::Edit || !s.floating) return home;
    const bool left = e.kind == Element::StickLEl;
    if (m_scene.mode == Mode::Play && (left ? v.grabL : v.grabR)) return StickPose{left ? v.lox : v.rox, left ? v.loy : v.roy, true};
    return StickPose{s.cx, s.cy, s.hint};
}

// Whether a control is drawn at all right now.
bool Renderer::elementShown(const Element& e) const {
    if (e.kind == Element::MenuEl) return true;
    if (m_scene.mode == Mode::Hidden) return false;
    if (m_scene.mode == Mode::Edit) return true;  // hidden ones appear as ghosts so they can be brought back
    const Layout& l = m_scene.layout;
    switch (e.kind) {
    case Element::ButtonEl: return l.buttons[e.index].visible;
    case Element::StickLEl: return l.stickL.visible && e.pose.shown;
    case Element::StickREl: return l.stickR.visible && e.pose.shown;
    case Element::DpadEl: return l.dpad.visible;
    case Element::MenuEl: break;
    }
    return true;
}

// A button is lit while a finger is on it, and while the pad buttons it stands for are down
// (which keeps a latched toggle lit after the finger has left).
bool Renderer::buttonLit(int index, const Visual& v) const {
    const ButtonDef& b = m_scene.layout.buttons[index];
    return ((v.touched >> index) & 1u) || (b.bit && (v.pad.buttons & b.bit) == b.bit);
}

bool Renderer::elementSelected(const Element& e) const {
    const ControlRef sel = m_scene.selected;
    switch (e.kind) {
    case Element::ButtonEl: return sel.kind == ControlRef::ButtonC && sel.index == e.index;
    case Element::StickLEl: return sel.kind == ControlRef::StickL;
    case Element::StickREl: return sel.kind == ControlRef::StickR;
    case Element::DpadEl: return sel.kind == ControlRef::Dpad;
    case Element::MenuEl: break;
    }
    return false;
}

uint32_t Renderer::knobKey(const StickDef& s, int16_t ax, int16_t ay) const {
    const float travel = s.baseRadius * (1.0f - kKnobRatio) * m_canvas.width() / kScreenW;
    const int kx = static_cast<int>(std::lround(ax * travel / kStickMax));
    const int ky = static_cast<int>(std::lround(-ay * travel / kStickMax));
    return packKnob(kx, ky) | ((ax || ay) ? 1u << 30 : 0u);
}

uint32_t Renderer::keyFor(const Element& e, const Visual& v) const {
    const Layout& l = m_scene.layout;
    switch (e.kind) {
    case Element::ButtonEl: return buttonLit(e.index, v) ? 1 : 0;
    case Element::StickLEl: return knobKey(l.stickL, v.pad.lx, v.pad.ly);
    case Element::StickREl: return knobKey(l.stickR, v.pad.rx, v.pad.ry);
    case Element::DpadEl: return v.pad.buttons & kDpadBits;
    case Element::MenuEl: return v.menuHold;
    }
    return 0;
}

Rect Renderer::knobBounds(const Element& e, uint32_t key) const {
    const StickDef& s = stickOf(e);
    const float inv = static_cast<float>(kScreenW) / m_canvas.width();
    const int cx = e.pose.x + static_cast<int>(std::lround(knobX(key) * inv));
    const int cy = e.pose.y + static_cast<int>(std::lround(knobY(key) * inv));
    const int r = static_cast<int>(s.baseRadius * kKnobRatio) + 1;
    return around(cx, cy, r, r);
}

Rect Renderer::dirtyFor(const Element& e, uint32_t oldKey, uint32_t newKey) const {
    // A moving knob only needs the area it left and the area it entered.
    if (e.kind == Element::StickLEl || e.kind == Element::StickREl) return unite(knobBounds(e, oldKey), knobBounds(e, newKey));
    return e.bounds;
}

// Largest size up to maxSize at which the text is no wider than maxWidth.
float Renderer::fitText(const char* utf8, float maxSize, float maxWidth) const {
    if (!m_font || !m_font->ready()) return 0;
    const float width = m_font->width(m_canvas, maxSize, utf8);
    return width <= maxWidth ? maxSize : maxSize * maxWidth / width;
}

void Renderer::text(float x, float baseline, float size, const char* utf8, Color c) {
    if (m_font && m_font->ready()) m_font->draw(m_canvas, x, baseline, size, utf8, c);
}

void Renderer::textCentered(float cx, float baseline, float size, const char* utf8, Color c) {
    if (!m_font || !m_font->ready()) return;
    m_font->draw(m_canvas, cx - m_font->width(m_canvas, size, utf8) * 0.5f, baseline, size, utf8, c);
}

// Button captions are small on a half-resolution layer; a second pass a fraction of a pixel
// to the side thickens the strokes enough to read over a busy game picture.
void Renderer::caption(float cx, float baseline, float size, const char* utf8, Color c) {
    if (!m_font || !m_font->ready()) return;
    const float x = cx - m_font->width(m_canvas, size, utf8) * 0.5f;
    const float shift = 0.5f * kScreenW / m_canvas.width();
    m_font->draw(m_canvas, x - shift * 0.5f, baseline, size, utf8, c);
    m_font->draw(m_canvas, x + shift * 0.5f, baseline, size, utf8, c);
}

void Renderer::drawButton(const ButtonDef& b, bool pressed, const Style& st) {
    const Color fill = pressed ? st.fillDown : st.fill;
    const Color line = pressed ? st.lineDown : st.line;
    const Color glyph = pressed ? st.glyphDown : st.glyph;
    // A dark rim just outside the ring keeps the button readable on a bright picture.
    const bool round = b.shape == Shape::Circle;
    constexpr float kRim = 2.0f, kRimWidth = 2.0f;  // stays within kPad of the button
    if (round) {
        m_canvas.strokeCircle(b.cx, b.cy, b.rx + kRim, kRimWidth, st.shadow);
        m_canvas.fillCircle(b.cx, b.cy, b.rx, fill);
        m_canvas.strokeCircle(b.cx, b.cy, b.rx, st.outline, line);
    } else {
        const float radius = (b.rx < b.ry ? b.rx : b.ry) * 0.55f;
        m_canvas.strokeRoundRect(b.cx, b.cy, b.rx + kRim, b.ry + kRim, radius + kRim, kRimWidth, st.shadow);
        m_canvas.fillRoundRect(b.cx, b.cy, b.rx, b.ry, radius, fill);
        m_canvas.strokeRoundRect(b.cx, b.cy, b.rx, b.ry, radius, st.outline, line);
    }

    const float box = round ? b.rx * 0.58f : b.ry * 0.5f;       // half height of the pictogram
    const float room = round ? b.rx * 1.6f : b.rx * 2.0f - 22;  // width a caption may take
    const bool hasGlyph = b.glyph != Glyph::None;
    if (!b.label[0]) {
        drawGlyph(m_canvas, b.glyph, b.cx, b.cy, box, glyph);
    } else if (!hasGlyph) {
        const float size = fitText(b.label, round ? b.rx * 0.7f : b.ry * 1.1f, room);
        if (size > 0) caption(b.cx, b.cy + size * kBaselineShift, size, b.label, glyph);
    } else {
        // Pictogram above its caption; a caption too small to read is left out.
        const float size = fitText(b.label, box * 0.62f, room * 0.86f);
        if (size >= kMinCaption) {
            drawGlyph(m_canvas, b.glyph, b.cx, b.cy - box * 0.34f, box * 0.72f, glyph);
            caption(b.cx, b.cy + box * 1.0f, size, b.label, glyph);
        } else {
            drawGlyph(m_canvas, b.glyph, b.cx, b.cy, box, glyph);
        }
    }
}

void Renderer::drawStick(const Element& e, int16_t ax, int16_t ay, const Style& st) {
    const StickDef& s = stickOf(e);
    m_canvas.fillCircle(e.pose.x, e.pose.y, s.baseRadius, st.baseFill);
    m_canvas.strokeCircle(e.pose.x, e.pose.y, s.baseRadius, kOutline, st.baseLine);

    // Draw the knob on the same whole-pixel grid the repaint key is quantised to.
    const uint32_t key = knobKey(s, ax, ay);
    const float inv = static_cast<float>(kScreenW) / m_canvas.width();
    const float kx = e.pose.x + knobX(key) * inv;
    const float ky = e.pose.y + knobY(key) * inv;
    const float kr = s.baseRadius * kKnobRatio;
    const bool active = ax || ay;
    m_canvas.fillCircle(kx, ky, kr, active ? st.knobActive : st.knobIdle);
    m_canvas.strokeCircle(kx, ky, kr, kOutline, active ? st.lineDown : st.line);
}

void Renderer::drawDpad(uint32_t buttons, const Style& st) {
    const DpadDef& d = m_scene.layout.dpad;
    const float len = d.armLen;
    const float half = d.armHalfWidth;
    const float corner = half * 0.35f;
    m_canvas.fillCross(d.cx, d.cy, len, half, corner, st.fill);

    // Light the outer part of each pressed arm, then put a chevron on every arm.
    const float inset = 6.0f;
    const float armMid = (half + len) * 0.5f;          // centre of the part beyond the hub
    const float armHalfLen = (len - half) * 0.5f - inset;
    const float armHalfThick = half - inset;
    const struct { uint32_t bit; float dx, dy; } arms[] = {
        {BtnRight, 1, 0}, {BtnLeft, -1, 0}, {BtnDown, 0, 1}, {BtnUp, 0, -1}};
    for (const auto& a : arms) {
        const bool pressed = buttons & a.bit;
        const float ax = d.cx + a.dx * armMid;
        const float ay = d.cy + a.dy * armMid;
        if (pressed) {
            const float hw = a.dx != 0 ? armHalfLen : armHalfThick;
            const float hh = a.dx != 0 ? armHalfThick : armHalfLen;
            m_canvas.fillRoundRect(ax, ay, hw, hh, corner, st.fillDown);
        }
        // Chevron pointing outwards: two strokes meeting at the tip.
        const float t = half * 0.42f;
        const float tipX = ax + a.dx * t * 0.6f, tipY = ay + a.dy * t * 0.6f;
        const float backX = ax - a.dx * t * 0.6f, backY = ay - a.dy * t * 0.6f;
        const float sideX = a.dy * t, sideY = a.dx * t;  // perpendicular
        const Color col = pressed ? st.glyphDown : st.glyph;
        m_canvas.line(backX + sideX, backY + sideY, tipX, tipY, 5.0f, col);
        m_canvas.line(backX - sideX, backY - sideY, tipX, tipY, 5.0f, col);
    }
    m_canvas.strokeCross(d.cx, d.cy, len, half, corner, kOutline, st.line);
}

void Renderer::drawMenuButton(const Visual& v) {
    const MenuDef& m = m_scene.layout.menu;
    const Theme& t = m_scene.theme;
    const Style st = styleFor(t.r, t.g, t.b, false);
    const bool open = m_scene.mode == Mode::Menu || m_scene.mode == Mode::Edit;
    if (v.menuHold > 0) {
        // Fills up while held, so a long press visibly "charges" towards leaving the overlay.
        const uint8_t a = static_cast<uint8_t>(3 * v.menuHold > 15 ? 15 : 3 * v.menuHold);
        m_canvas.fillCircle(m.cx, m.cy, m.r, Color{t.r, t.g, t.b, a});
    } else {
        m_canvas.fillCircle(m.cx, m.cy, m.r, open ? st.fillDown : (v.paused ? Color{0, 0, 0, 6} : st.baseFill));
    }
    m_canvas.strokeCircle(m.cx, m.cy, m.r, kOutline, (v.paused || open) ? st.glyph : st.baseLine);
    strokes(m_canvas, kGlyphMenu, m.cx, m.cy, m.r * 0.55f, (v.menuHold >= 3 || open) ? st.glyphDown : st.line);
}

void Renderer::drawSelection(const Element& e) {
    const Layout& l = m_scene.layout;
    const float w = 5.0f, gap = 7.0f;
    switch (e.kind) {
    case Element::ButtonEl: {
        const ButtonDef& b = l.buttons[e.index];
        if (b.shape == Shape::Circle) m_canvas.strokeCircle(b.cx, b.cy, b.rx + gap, w, kSelection);
        else m_canvas.strokeRoundRect(b.cx, b.cy, b.rx + gap, b.ry + gap, (b.rx < b.ry ? b.rx : b.ry) * 0.55f + gap, w, kSelection);
        break;
    }
    case Element::StickLEl: m_canvas.strokeCircle(l.stickL.cx, l.stickL.cy, l.stickL.baseRadius + gap, w, kSelection); break;
    case Element::StickREl: m_canvas.strokeCircle(l.stickR.cx, l.stickR.cy, l.stickR.baseRadius + gap, w, kSelection); break;
    case Element::DpadEl: m_canvas.strokeCircle(l.dpad.cx, l.dpad.cy, l.dpad.armLen + gap, w, kSelection); break;
    case Element::MenuEl: break;
    }
}

void Renderer::drawElement(const Element& e, const Visual& v) {
    const Layout& l = m_scene.layout;
    const Theme& t = m_scene.theme;
    switch (e.kind) {
    case Element::ButtonEl: {
        const ButtonDef& b = l.buttons[e.index];
        drawButton(b, buttonLit(e.index, v), styleForButton(b));
        break;
    }
    case Element::StickLEl: drawStick(e, v.pad.lx, v.pad.ly, styleFor(t.r, t.g, t.b, !l.stickL.visible)); break;
    case Element::StickREl: drawStick(e, v.pad.rx, v.pad.ry, styleFor(t.r, t.g, t.b, !l.stickR.visible)); break;
    case Element::DpadEl: drawDpad(v.pad.buttons, styleFor(t.r, t.g, t.b, !l.dpad.visible)); break;
    case Element::MenuEl: drawMenuButton(v); break;
    }
    if (m_scene.mode == Mode::Edit && elementSelected(e)) drawSelection(e);
}

void Renderer::drawMenu() {
    const Theme& t = m_scene.theme;
    const Color accent{t.r, t.g, t.b, 15};
    const Color highlight{t.r, t.g, t.b, 5};
    const Rect p = ui::kMenuPanel;
    const float pw = (p.x1 - p.x0) * 0.5f, ph = (p.y1 - p.y0) * 0.5f;
    m_canvas.fillRoundRect(midX(p), midY(p), pw, ph, 24, kPanelFill);
    m_canvas.strokeRoundRect(midX(p), midY(p), pw, ph, 24, kOutline, Color{t.r, t.g, t.b, 9});

    for (int row = 0; row < kMenuRows; row++) {
        const Rect r = ui::menuRow(row);
        const float cy = midY(r);
        const float baseline = cy + kLabelSize * kBaselineShift;
        const bool pressed = m_scene.pressedRow == row;

        if (pressed && m_scene.pressedArrow == 0) {
            m_canvas.fillRoundRect(midX(r), cy, (r.x1 - r.x0) * 0.5f - 2, (r.y1 - r.y0) * 0.5f - 4, 12, highlight);
        }
        text(r.x0 + 20, baseline, kLabelSize, ui::rowLabel(row), kTextColor);

        if (ui::rowHasArrows(row)) {
            const Rect left = ui::menuArrow(row, false), right = ui::menuArrow(row, true);
            for (int side = -1; side <= 1; side += 2) {
                const Rect a = side < 0 ? left : right;
                if (pressed && m_scene.pressedArrow == side) {
                    m_canvas.fillRoundRect(midX(a), cy, (a.x1 - a.x0) * 0.5f - 4, (a.y1 - a.y0) * 0.5f - 6, 12, highlight);
                }
                // Chevron pointing away from the value.
                const float tip = midX(a) + side * 9.0f, back = midX(a) - side * 7.0f;
                m_canvas.line(back, cy - 14, tip, cy, 5.0f, accent);
                m_canvas.line(back, cy + 14, tip, cy, 5.0f, accent);
            }
            textCentered((left.x1 + right.x0) * 0.5f, baseline, kLabelSize, m_scene.rowValue[row], accent);
        } else if (m_scene.rowValue[row][0] && m_font && m_font->ready()) {
            const float w = m_font->width(m_canvas, kLabelSize, m_scene.rowValue[row]);
            text(r.x1 - 24 - w, baseline, kLabelSize, m_scene.rowValue[row], accent);
        }
    }
}

void Renderer::drawEditor() {
    const Theme& t = m_scene.theme;
    const Color accent{t.r, t.g, t.b, 15};
    const Rect bar = ui::kToolbar;
    m_canvas.fillRoundRect(midX(bar), midY(bar), (bar.x1 - bar.x0) * 0.5f, (bar.y1 - bar.y0) * 0.5f, 20, kPanelFill);
    m_canvas.strokeRoundRect(midX(bar), midY(bar), (bar.x1 - bar.x0) * 0.5f, (bar.y1 - bar.y0) * 0.5f, 20, kOutline, Color{t.r, t.g, t.b, 9});

    // Caption: which layout of which pack is being edited, and what to do.
    const bool hasSelection = m_scene.selected.kind != ControlRef::None;
    char title[72];
    if (std::strcmp(m_scene.packName, m_scene.layoutName) == 0) std::snprintf(title, sizeof(title), "%s", m_scene.packName);
    else std::snprintf(title, sizeof(title), "%s · %s", m_scene.packName, m_scene.layoutName);
    textCentered(midX(bar), bar.y0 + 30, fitText(title, kToolSize, bar.x1 - bar.x0 - 32.0f), title, kTextColor);
    textCentered(midX(bar), bar.y0 + 54, 21.0f, hasSelection ? "size and visibility: buttons below" : "drag a control · tap to select",
                 Color{15, 15, 15, 9});

    bool selectedShown = true;
    for (int i = 0; i < m_elementCount; i++) {
        const Element& e = m_elements[i];
        if (!elementSelected(e)) continue;
        const Layout& l = m_scene.layout;
        selectedShown = e.kind == Element::ButtonEl ? l.buttons[e.index].visible
                      : e.kind == Element::StickLEl ? l.stickL.visible
                      : e.kind == Element::StickREl ? l.stickR.visible : l.dpad.visible;
    }

    for (int tool = 0; tool < kEditTools; tool++) {
        const Rect r = ui::toolButton(tool);
        const bool needsSelection = tool == ToolSmaller || tool == ToolBigger || tool == ToolShow;
        const bool enabled = !needsSelection || hasSelection;
        const bool pressed = m_scene.pressedTool == tool;
        const float hw = (r.x1 - r.x0) * 0.5f, hh = (r.y1 - r.y0) * 0.5f;
        if (pressed && enabled) m_canvas.fillRoundRect(midX(r), midY(r), hw, hh, 12, Color{t.r, t.g, t.b, 6});
        m_canvas.strokeRoundRect(midX(r), midY(r), hw, hh, 12, kOutline, Color{t.r, t.g, t.b, static_cast<uint8_t>(enabled ? 10 : 3)});
        const Color label = enabled ? (needsSelection ? kSelection : accent) : Color{15, 15, 15, 4};
        textCentered(midX(r), midY(r) + kToolSize * kBaselineShift, kToolSize, ui::toolLabel(tool, selectedShown), label);
    }
}

void Renderer::repaint(const Rect& area, const Visual& v) {
    m_canvas.setClip(area);
    m_canvas.clear();
    // Everything overlapping the area is redrawn, in a fixed order, so the result never
    // depends on which control triggered the repaint.
    for (int i = 0; i < m_elementCount; i++) {
        const Element& e = m_elements[i];
        if (elementShown(e) && e.bounds.intersects(area)) drawElement(e, v);
    }
    if (m_scene.mode == Mode::Menu && ui::kMenuPanel.intersects(area)) drawMenu();
    if (m_scene.mode == Mode::Edit && ui::kToolbar.intersects(area)) drawEditor();
    m_canvas.resetClip();
}

bool Renderer::draw(const Scene& scene) {
    const bool restructure = m_first || scene.revision != m_scene.revision || scene.mode != m_scene.mode;
    if (restructure) {
        m_first = false;
        m_scene = scene;
        rebuildElements();
        for (int i = 0; i < m_elementCount; i++) m_elements[i].key = keyFor(m_elements[i], scene.visual);
        repaint(whole(), scene.visual);
        return true;
    }

    // Same layout, theme and menu state as on the canvas: only the pad state can differ.
    // While the menu or the editor is up the pad is frozen, so only the menu button
    // (hold-to-leave feedback) is live.
    const bool padLive = m_scene.mode == Mode::Play;
    m_scene.visual = scene.visual;
    bool changed = false;
    for (int i = 0; i < m_elementCount; i++) {
        Element& e = m_elements[i];
        if (padLive && (e.kind == Element::StickLEl || e.kind == Element::StickREl)) {
            // A floating stick jumps to the thumb that grabs it and follows it around.
            const StickPose pose = poseFor(e, scene.visual);
            if (!(pose == e.pose)) {
                const bool was = elementShown(e);
                const Rect old = e.bounds;
                e.pose = pose;
                e.bounds = around(pose.x, pose.y, stickOf(e).baseRadius, stickOf(e).baseRadius);
                e.key = keyFor(e, scene.visual);
                const bool is = elementShown(e);
                if (was || is) {
                    repaint(was && is ? unite(old, e.bounds) : (was ? old : e.bounds), scene.visual);
                    changed = true;
                }
                continue;
            }
        }
        if (!elementShown(e)) continue;
        if (!padLive && e.kind != Element::MenuEl) continue;
        const uint32_t key = keyFor(e, scene.visual);
        if (key == e.key) continue;
        const Rect area = dirtyFor(e, e.key, key);
        e.key = key;
        repaint(area, scene.visual);
        changed = true;
    }
    return changed;
}

}  // namespace tp
