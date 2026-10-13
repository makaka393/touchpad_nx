#include "tinytest.hpp"
#include "../core/layout.hpp"

#include <cmath>

using namespace tp;

namespace {

struct Zone { const char* name; int cx, cy, r; };

double dist(int ax, int ay, int bx, int by) { return std::hypot(ax - bx, ay - by); }

// Distance from a point to the nearest point of a button's hit area (0 if inside).
double gapToButton(const ButtonDef& b, int slop, int px, int py) {
    if (b.shape == Shape::Circle) {
        const double g = dist(b.cx, b.cy, px, py) - (b.rx + slop);
        return g > 0 ? g : 0;
    }
    const double dx = std::fabs(px - b.cx) - (b.rx + slop);
    const double dy = std::fabs(py - b.cy) - (b.ry + slop);
    return std::hypot(dx > 0 ? dx : 0, dy > 0 ? dy : 0);
}

}  // namespace

TEST(default_layout_has_every_gamepad_button_exactly_once) {
    const Layout& l = defaultLayout();
    const uint32_t want = BtnA | BtnB | BtnX | BtnY | BtnL | BtnR | BtnZL | BtnZR | BtnPlus | BtnMinus |
                          BtnStickL | BtnStickR | BtnHome | BtnCapture;
    uint32_t seen = 0;
    for (int i = 0; i < l.buttonCount; i++) {
        CHECK((seen & l.buttons[i].bit) == 0);
        seen |= l.buttons[i].bit;
    }
    CHECK_EQ(seen, want);
}

TEST(default_layout_fits_on_screen) {
    const Layout& l = defaultLayout();
    for (int i = 0; i < l.buttonCount; i++) {
        const ButtonDef& b = l.buttons[i];
        CHECK(b.cx - b.rx >= 0 && b.cx + b.rx <= kScreenW);
        CHECK(b.cy - b.ry >= 0 && b.cy + b.ry <= kScreenH);
    }
    for (const StickDef* s : {&l.stickL, &l.stickR}) {
        CHECK(s->cx - s->baseRadius >= 0 && s->cx + s->baseRadius <= kScreenW);
        CHECK(s->cy - s->baseRadius >= 0 && s->cy + s->baseRadius <= kScreenH);
        CHECK(s->throwRadius > 0);
    }
    CHECK(l.dpad.cx - l.dpad.armLen >= 0 && l.dpad.cy + l.dpad.armLen <= kScreenH);
    CHECK(l.menu.cy - l.menu.r >= 0);
}

// A touch must never be ambiguous between a grab zone (stick / d-pad) and anything else.
TEST(default_layout_grab_zones_do_not_touch_each_other_or_buttons) {
    const Layout& l = defaultLayout();
    const Zone zones[] = {
        {"stickL", l.stickL.cx, l.stickL.cy, l.stickL.zoneRadius},
        {"stickR", l.stickR.cx, l.stickR.cy, l.stickR.zoneRadius},
        {"dpad", l.dpad.cx, l.dpad.cy, l.dpad.zoneRadius},
    };
    for (int i = 0; i < 3; i++) {
        for (int j = i + 1; j < 3; j++) {
            CHECK(dist(zones[i].cx, zones[i].cy, zones[j].cx, zones[j].cy) > zones[i].r + zones[j].r);
        }
        for (int b = 0; b < l.buttonCount; b++) {
            CHECK(gapToButton(l.buttons[b], l.slop, zones[i].cx, zones[i].cy) > zones[i].r);
        }
        CHECK(dist(zones[i].cx, zones[i].cy, l.menu.cx, l.menu.cy) > zones[i].r + l.menu.r + l.slop);
    }
}

// Only the four face buttons may be pressed two at a time with one thumb.
TEST(default_layout_only_face_buttons_share_hit_area) {
    const Layout& l = defaultLayout();
    const uint32_t face = BtnA | BtnB | BtnX | BtnY;
    for (int i = 0; i < l.buttonCount; i++) {
        for (int j = i + 1; j < l.buttonCount; j++) {
            const ButtonDef& a = l.buttons[i];
            const ButtonDef& b = l.buttons[j];
            if ((a.bit & face) && (b.bit & face)) continue;
            // Treat b as a point grown by its own hit size: conservative for circles and rects alike.
            const int reach = (b.rx > b.ry ? b.rx : b.ry) + l.slop;
            CHECK(gapToButton(a, l.slop, b.cx, b.cy) > reach);
        }
        CHECK(gapToButton(l.buttons[i], l.slop, l.menu.cx, l.menu.cy) > l.menu.r + l.slop);
    }
}

TEST(default_layout_adjacent_face_buttons_can_be_rocked_together) {
    const Layout& l = defaultLayout();
    const ButtonDef* a = nullptr;
    const ButtonDef* b = nullptr;
    for (int i = 0; i < l.buttonCount; i++) {
        if (l.buttons[i].bit == BtnA) a = &l.buttons[i];
        if (l.buttons[i].bit == BtnB) b = &l.buttons[i];
    }
    CHECK(a && b);
    if (!a || !b) return;
    const int mx = (a->cx + b->cx) / 2;
    const int my = (a->cy + b->cy) / 2;
    CHECK_EQ(gapToButton(*a, l.slop, mx, my), 0);
    CHECK_EQ(gapToButton(*b, l.slop, mx, my), 0);
}
