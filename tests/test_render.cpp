#include "tinytest.hpp"
#include "../core/render.hpp"

#include <vector>

using namespace tp;

namespace {

const Layout kLayout = {
    {
        {BtnA, Shape::Circle, 1000, 300, 50, 50, Glyph::A},
        {BtnB, Shape::Circle, 920, 380, 50, 50, Glyph::B},
        {BtnZL, Shape::RoundRect, 100, 60, 80, 40, Glyph::ZL},
    },
    3,
    {200, 300, 100, 140, 80},
    {900, 560, 100, 120, 80},
    {350, 560, 100, 36, 120, 20},
    {640, 50, 30},
    14,
};

// Two buttons whose drawings genuinely overlap.
const Layout kCrowded = {
    {
        {BtnA, Shape::Circle, 1000, 300, 50, 50, Glyph::A},
        {BtnB, Shape::Circle, 1060, 300, 50, 50, Glyph::B},
    },
    2,
    {200, 300, 100, 140, 80},
    {900, 560, 100, 120, 80},
    {350, 560, 100, 36, 120, 20},
    {640, 50, 30},
    14,
};

// Drives the renderer the way the overlay does: a Scene whose layout stays put while the
// pad state changes.
struct Screen {
    std::vector<uint16_t> px = std::vector<uint16_t>(640 * 360, 0);
    Canvas canvas{px.data(), 640, 360, 640};
    Scene scene;

    struct Adapter {
        Screen& s;
        Renderer r;
        bool draw(const Visual& v) {
            s.scene.visual = v;
            s.scene.mode = v.paused ? Mode::Hidden : Mode::Play;
            return r.draw(s.scene);
        }
    } renderer;

    explicit Screen(const Layout& layout = kLayout) : renderer{*this, Renderer(canvas)} { scene.layout = layout; }

    int alpha(int fx, int fy) const { return canvas.pixel(fx, fy) >> 12; }
};

Visual pressed(uint32_t buttons) {
    Visual v;
    v.pad.buttons = buttons;
    return v;
}

// Bounding box (framebuffer pixels) of every pixel that differs between two frames.
struct Diff {
    int count = 0;
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
};

Diff diff(const std::vector<uint16_t>& a, const std::vector<uint16_t>& b) {
    Diff d;
    for (int y = 0; y < 360; y++) {
        for (int x = 0; x < 640; x++) {
            if (a[y * 640 + x] == b[y * 640 + x]) continue;
            d.count++;
            if (x < d.x0) d.x0 = x;
            if (y < d.y0) d.y0 = y;
            if (x > d.x1) d.x1 = x;
            if (y > d.y1) d.y1 = y;
        }
    }
    return d;
}

}  // namespace

TEST(first_draw_paints_controls_and_leaves_gaps_clear) {
    Screen s;
    CHECK(s.renderer.draw(Visual{}));
    CHECK(s.alpha(500, 150) > 0);   // inside button A
    CHECK(s.alpha(100, 150) > 0);   // inside the left stick
    CHECK(s.alpha(175, 280) > 0);   // d-pad centre
    CHECK(s.alpha(320, 40) > 0 || s.alpha(320, 11) > 0 || s.alpha(334, 25) > 0);  // menu button
    CHECK_EQ(s.canvas.pixel(320, 180), 0);  // middle of the screen stays see-through
}

TEST(unchanged_state_repaints_nothing) {
    Screen s;
    s.renderer.draw(Visual{});
    const auto before = s.px;
    CHECK(!s.renderer.draw(Visual{}));
    CHECK(s.px == before);
}

TEST(pressing_a_button_repaints_only_that_button) {
    Screen s;
    s.renderer.draw(Visual{});
    const auto idle = s.px;
    CHECK(s.renderer.draw(pressed(BtnA)));
    const Diff d = diff(idle, s.px);
    CHECK(d.count > 500);  // the whole disc lights up, not a few pixels
    // Button A occupies pixels x 475..525, y 125..175; allow 2 px for the outline and AA.
    CHECK(d.x0 >= 473 && d.x1 <= 527);
    CHECK(d.y0 >= 123 && d.y1 <= 177);
}

TEST(releasing_restores_the_idle_picture_exactly) {
    Screen s;
    s.renderer.draw(Visual{});
    const auto idle = s.px;
    s.renderer.draw(pressed(BtnA | BtnZL));
    s.renderer.draw(Visual{});
    CHECK(s.px == idle);
}

TEST(repainting_one_button_keeps_an_overlapping_neighbour_intact) {
    Screen s(kCrowded);
    s.renderer.draw(Visual{});
    const auto idle = s.px;
    s.renderer.draw(pressed(BtnA));
    s.renderer.draw(Visual{});
    CHECK(s.px == idle);
}

TEST(stick_knob_follows_deflection_inside_its_base) {
    Screen s;
    s.renderer.draw(Visual{});
    const auto idle = s.px;
    Visual v;
    v.pad.lx = 32767;
    CHECK(s.renderer.draw(v));
    const Diff d = diff(idle, s.px);
    CHECK(d.count > 100);
    // Left stick base: centre (100,150) px, radius 50 px.
    CHECK(d.x0 >= 48 && d.x1 <= 152);
    CHECK(d.y0 >= 98 && d.y1 <= 202);
    CHECK(d.x1 > 130);  // something was painted on the right-hand side
    s.renderer.draw(Visual{});
    CHECK(s.px == idle);
}

TEST(tiny_stick_changes_below_a_pixel_do_not_repaint) {
    Screen s;
    Visual v;
    v.pad.lx = 16000;
    s.renderer.draw(v);
    v.pad.lx = 16010;
    CHECK(!s.renderer.draw(v));
}

TEST(dpad_direction_lights_its_arm) {
    Screen s;
    s.renderer.draw(Visual{});
    const auto idle = s.px;
    s.renderer.draw(pressed(BtnRight));
    const Diff d = diff(idle, s.px);
    CHECK(d.count > 100);
    CHECK(d.x0 >= 175);  // d-pad centre is x=175 px: only the right arm changes
    s.renderer.draw(Visual{});
    CHECK(s.px == idle);
}

TEST(paused_hides_everything_but_the_menu_button) {
    Screen s;
    s.renderer.draw(Visual{});
    const auto idle = s.px;
    Visual v;
    v.paused = true;
    CHECK(s.renderer.draw(v));
    CHECK_EQ(s.canvas.pixel(500, 150), 0);  // button A gone
    CHECK_EQ(s.canvas.pixel(100, 150), 0);  // left stick gone
    int menuPixels = 0;
    for (int y = 5; y < 45; y++)
        for (int x = 300; x < 340; x++) menuPixels += s.alpha(x, y) > 0;
    CHECK(menuPixels > 50);
    s.renderer.draw(Visual{});
    CHECK(s.px == idle);
}

TEST(menu_hold_progress_is_visible) {
    Screen s;
    s.renderer.draw(Visual{});
    const auto idle = s.px;
    Visual v;
    v.menuHold = 3;
    CHECK(s.renderer.draw(v));
    const Diff d = diff(idle, s.px);
    CHECK(d.count > 20);
    CHECK(d.x0 >= 300 && d.x1 <= 340 && d.y1 <= 45);  // confined to the menu button (centre 320,25 px)
}
