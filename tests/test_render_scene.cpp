#include "tinytest.hpp"
#include "../core/render.hpp"

#include <cstdio>
#include <vector>

using namespace tp;

namespace {

std::vector<unsigned char> loadFont() {
    std::vector<unsigned char> d;
    if (FILE* f = std::fopen("/usr/share/fonts/TTF/DejaVuSans.ttf", "rb")) {
        std::fseek(f, 0, SEEK_END);
        d.resize(static_cast<size_t>(std::ftell(f)));
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(d.data(), 1, d.size(), f) != d.size()) d.clear();
        std::fclose(f);
    }
    return d;
}

// Default layout landmarks in framebuffer pixels (logical / 2):
//   A centre (601,150) r26, so (627,150) lies on its outline; right stick centre (475,282);
//   ZL is the rounded rect centred (46,31), half extents 39x23; (46,7) lies on its outline, just
//   outside the dark fill.
struct Stage {
    std::vector<unsigned char> fontData = loadFont();
    Font font;
    std::vector<uint16_t> px = std::vector<uint16_t>(640 * 360, 0);
    Canvas canvas{px.data(), 640, 360, 640};
    Renderer renderer;
    Scene scene;

    explicit Stage(bool withFont = true) : renderer(canvas, withFont ? &font : nullptr) {
        font.load(fontData.data());
        scene.layout = defaultLayout();
        std::snprintf(scene.packName, sizeof(scene.packName), "All buttons");
        std::snprintf(scene.rowValue[RowPack], 32, "All buttons");
        std::snprintf(scene.rowValue[RowColor], 32, "White");
        std::snprintf(scene.rowValue[RowOpacity], 32, "4 / 5");
        std::snprintf(scene.rowValue[RowFaceColors], 32, "off");
    }

    bool draw() { return renderer.draw(scene); }
    bool redraw() {  // something structural changed
        scene.revision++;
        return renderer.draw(scene);
    }
    uint16_t at(int x, int y) const { return canvas.pixel(x, y); }
    int alpha(int x, int y) const { return at(x, y) >> 12; }
    int rgb(int x, int y) const { return at(x, y) & 0xFFF; }

    // Bright opaque pixels (text) inside a logical rectangle. The panel behind is black and
    // not fully opaque, so it never counts.
    int ink(const Rect& r) const {
        int n = 0;
        for (int y = r.y0 / 2; y < r.y1 / 2; y++)
            for (int x = r.x0 / 2; x < r.x1 / 2; x++) n += alpha(x, y) == 15 && (at(x, y) & 0xF) >= 8;
        return n;
    }
};

// A colour sampled on a control's edge, give or take a few steps per channel: the edge is
// thinner than a pixel there and blends with the dark rim next to it.
bool closeTo(int rgb, int want) {
    for (int shift = 0; shift < 12; shift += 4) {
        const int d = ((rgb >> shift) & 15) - ((want >> shift) & 15);
        if (d < -3 || d > 3) return false;
    }
    return true;
}

struct Diff {
    int count = 0;
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
};

Diff diff(const std::vector<uint16_t>& a, const std::vector<uint16_t>& b) {
    Diff d;
    for (int y = 0; y < 360; y++)
        for (int x = 0; x < 640; x++) {
            if (a[y * 640 + x] == b[y * 640 + x]) continue;
            d.count++;
            if (x < d.x0) d.x0 = x;
            if (y < d.y0) d.y0 = y;
            if (x > d.x1) d.x1 = x;
            if (y > d.y1) d.y1 = y;
        }
    return d;
}

}  // namespace

// ---- theme ----

TEST(accent_colour_tints_the_controls) {
    Stage s;
    s.scene.theme.r = 6;
    s.scene.theme.g = 15;
    s.scene.theme.b = 7;
    s.draw();
    CHECK(closeTo(s.rgb(627, 150), 0x7F6));  // b=7 g=15 r=6 on A's outline
    CHECK(closeTo(s.rgb(46, 7), 0x7F6));     // and on ZL's
}

TEST(lower_opacity_makes_the_controls_fainter) {
    Stage normal, faint;
    normal.draw();
    faint.scene.theme.opacity = 2;
    faint.draw();
    CHECK(faint.alpha(627, 150) > 0);
    CHECK(faint.alpha(627, 150) < normal.alpha(627, 150));
    CHECK(faint.alpha(601, 170) < normal.alpha(601, 170));  // the dark fill too
}

TEST(face_colours_apply_to_abxy_only) {
    Stage s;
    s.scene.theme.faceColors = true;
    s.draw();
    CHECK(s.rgb(627, 150) != 0xFFF);  // A is no longer the white accent
    CHECK(closeTo(s.rgb(627, 150), 0x44F)); // red: r=15 g=4 b=4
    CHECK(closeTo(s.rgb(46, 7), 0xFFF));    // ZL keeps the accent
}

// ---- packs: hidden controls ----

TEST(hidden_control_is_not_drawn_during_play) {
    Stage s;
    s.scene.layout.buttons[4].visible = false;  // A
    s.scene.layout.stickR.visible = false;
    s.draw();
    CHECK_EQ(s.at(601, 150), 0);
    CHECK_EQ(s.at(627, 150), 0);
    CHECK_EQ(s.at(475, 282), 0);
    CHECK(s.alpha(555, 196) > 0);  // B (1110,392) is still there
}

TEST(switching_pack_erases_controls_that_went_away) {
    Stage s;
    s.draw();
    CHECK(s.alpha(475, 282) > 0);
    s.scene.layout.stickR.visible = false;
    CHECK(s.redraw());
    CHECK_EQ(s.at(475, 282), 0);
}

TEST(moved_control_is_drawn_at_its_new_place_only) {
    Stage s;
    s.draw();
    s.scene.layout.buttons[4].cx = 700;  // A to the middle of the screen
    s.scene.layout.buttons[4].cy = 400;
    s.redraw();
    CHECK_EQ(s.at(601, 150), 0);
    CHECK(s.alpha(350, 215) > 0);
}

TEST(pressing_a_button_after_a_pack_switch_still_repaints_just_that_button) {
    Stage s;
    s.draw();
    s.scene.layout.buttons[4].cx = 700;
    s.scene.layout.buttons[4].cy = 400;
    s.redraw();
    const auto idle = s.px;
    s.scene.visual.pad.buttons = BtnA;
    CHECK(s.draw());
    const Diff d = diff(idle, s.px);
    CHECK(d.count > 500);
    CHECK(d.x0 >= 350 - 30 && d.x1 <= 350 + 30 && d.y0 >= 200 - 30 && d.y1 <= 200 + 30);
}

// ---- hidden mode ----

TEST(hidden_mode_shows_only_the_menu_button) {
    Stage s;
    s.scene.mode = Mode::Hidden;
    s.scene.visual.paused = true;
    s.draw();
    CHECK_EQ(s.at(601, 150), 0);
    CHECK_EQ(s.at(100, 150), 0);
    CHECK(s.alpha(320, 37) > 0 || s.alpha(320, 9) > 0 || s.alpha(334, 23) > 0);  // menu button at (640,46)
}

// ---- menu ----

TEST(menu_panel_is_drawn_over_the_pad_with_labels) {
    Stage s;
    s.scene.mode = Mode::Menu;
    s.draw();
    CHECK(s.alpha(320, 64) >= 12);                    // dark panel body (its top padding)
    CHECK(s.ink(ui::menuRow(RowHide)) > 60);          // "Hide buttons"
    CHECK(s.ink(ui::menuRow(RowUltrahand)) > 60);
    CHECK(s.alpha(601, 150) > 0);                           // pad still visible beside the panel
}

TEST(menu_without_a_font_still_draws_the_panel) {
    Stage s(false);
    s.scene.mode = Mode::Menu;
    CHECK(s.draw());
    CHECK(s.alpha(320, 64) >= 12);
    CHECK_EQ(s.ink(ui::menuRow(RowHide)), 0);
}

TEST(pressed_menu_row_is_highlighted) {
    Stage s;
    s.scene.mode = Mode::Menu;
    s.draw();
    const auto before = s.px;
    s.scene.pressedRow = RowEdit;
    CHECK(s.redraw());
    const Diff d = diff(before, s.px);
    const Rect row = ui::menuRow(RowEdit);
    CHECK(d.count > 300);
    CHECK(d.x0 >= row.x0 / 2 - 1 && d.x1 <= row.x1 / 2 + 1);
    CHECK(d.y0 >= row.y0 / 2 - 1 && d.y1 <= row.y1 / 2 + 1);
}

TEST(pressed_arrow_highlights_just_the_arrow) {
    Stage s;
    s.scene.mode = Mode::Menu;
    s.draw();
    const auto before = s.px;
    s.scene.pressedRow = RowColor;
    s.scene.pressedArrow = 1;
    s.redraw();
    const Diff d = diff(before, s.px);
    const Rect arrow = ui::menuArrow(RowColor, true);
    CHECK(d.count > 50);
    CHECK(d.x0 >= arrow.x0 / 2 - 1 && d.x1 <= arrow.x1 / 2 + 1);
}

TEST(menu_shows_the_current_values) {
    Stage a, b;
    a.scene.mode = b.scene.mode = Mode::Menu;
    std::snprintf(b.scene.rowValue[RowPack], 32, "Racing");
    a.draw();
    b.draw();
    const Diff d = diff(a.px, b.px);
    const Rect row = ui::menuRow(RowPack);
    CHECK(d.count > 30);
    CHECK(d.y0 >= row.y0 / 2 && d.y1 <= row.y1 / 2);
    CHECK(d.x0 >= ui::menuArrow(RowPack, false).x1 / 2 - 2);  // between the arrows, not over the label
}

TEST(unchanged_menu_is_not_repainted) {
    Stage s;
    s.scene.mode = Mode::Menu;
    s.draw();
    CHECK(!s.draw());
}

TEST(pad_presses_do_not_show_while_the_menu_is_open) {
    Stage s;
    s.scene.mode = Mode::Menu;
    s.draw();
    s.scene.visual.pad.buttons = BtnA;
    CHECK(!s.draw());
}

TEST(hold_feedback_on_the_menu_button_still_updates_inside_the_menu) {
    Stage s;
    s.scene.mode = Mode::Menu;
    s.draw();
    const auto before = s.px;
    s.scene.visual.menuHold = 3;
    CHECK(s.draw());
    const Diff d = diff(before, s.px);
    CHECK(d.x0 >= 300 && d.x1 <= 340 && d.y1 <= 45);
}

TEST(closing_the_menu_restores_the_plain_pad) {
    Stage s;
    s.draw();
    const auto pad = s.px;
    s.scene.mode = Mode::Menu;
    s.redraw();
    s.scene.mode = Mode::Play;
    s.redraw();
    CHECK(s.px == pad);
}

// ---- editor ----

TEST(editor_shows_hidden_controls_as_ghosts) {
    Stage s;
    s.scene.mode = Mode::Edit;
    s.scene.layout.buttons[4].visible = false;  // A
    s.draw();
    const int ghost = s.alpha(627, 150);
    const int solid = s.alpha(555 + 26, 196);   // B's outline
    CHECK(ghost > 0);
    CHECK(ghost < solid);
}

TEST(editor_marks_the_selected_control) {
    Stage plain, picked;
    plain.scene.mode = picked.scene.mode = Mode::Edit;
    picked.scene.selected = ControlRef{ControlRef::ButtonC, 4};
    plain.draw();
    picked.draw();
    // The ring sits just outside the button: 7 logical px beyond its radius.
    CHECK(picked.at(601 + 29, 150) != plain.at(601 + 29, 150));
    CHECK_EQ(picked.rgb(601 + 29, 150), 0x2DF);  // selection yellow: r=15 g=13 b=2
}

TEST(editor_toolbar_has_labelled_buttons) {
    Stage s;
    s.scene.mode = Mode::Edit;
    s.draw();
    const Rect bar = ui::kToolbar;
    CHECK(s.alpha((bar.x0 + 6) / 2, (bar.y0 + 100) / 2) >= 12);  // panel body, left margin
    // "Save", measured inside the button so its outline is not counted.
    const Rect save = ui::toolButton(ToolSave);
    CHECK(s.ink(Rect{save.x0 + 8, save.y0 + 8, save.x1 - 8, save.y1 - 8}) > 15);
}

TEST(editor_size_tools_look_disabled_without_a_selection) {
    Stage none, some;
    none.scene.mode = some.scene.mode = Mode::Edit;
    some.scene.selected = ControlRef{ControlRef::Dpad, 0};
    none.draw();
    some.draw();
    const Rect bigger = ui::toolButton(ToolBigger);
    const int x = bigger.x0 / 2, y = (bigger.y0 + bigger.y1) / 4;  // on the button's left edge
    CHECK(none.alpha(x, y) < some.alpha(x, y));
}

// ---- controls from game packs ----

namespace {

constexpr int kA = 4;  // index of A in the default layout; framebuffer centre (601,150), radius 26

bool insideA(const Diff& d) { return d.x0 >= 601 - 28 && d.x1 <= 601 + 28 && d.y0 >= 150 - 28 && d.y1 <= 150 + 28; }

// Left stick turned into a floating one; the d-pad is out of the way of the test spots.
void floatLeftStick(Scene& scene, bool hint) {
    StickDef& l = scene.layout.stickL;  // rests at (170,300): framebuffer (85,150)
    l.floating = true;
    l.hint = hint;
    l.zx0 = 0;
    l.zy0 = 100;
    l.zx1 = 560;
    l.zy1 = 720;
    scene.layout.dpad.visible = false;
}

void holdLeftStick(Scene& scene, int x, int y) {
    scene.visual.grabL = true;
    scene.visual.lox = static_cast<int16_t>(x);
    scene.visual.loy = static_cast<int16_t>(y);
}

}  // namespace

TEST(tinted_button_keeps_its_own_colour_whatever_the_theme) {
    Stage s;
    ButtonDef& a = s.scene.layout.buttons[kA];
    a.tinted = true;
    a.r = 4;
    a.g = 14;
    a.b = 6;
    s.scene.theme.r = 15;
    s.scene.theme.g = 5;
    s.scene.theme.b = 5;
    s.draw();
    CHECK(closeTo(s.rgb(627, 150), 0x6E4));  // A's ring in its own colour
    CHECK(closeTo(s.rgb(46, 7), 0x55F));     // ZL follows the theme
    CHECK_EQ(s.rgb(601, 172), 0x6E4);  // and so does its faint glass
    CHECK(s.alpha(601, 172) <= 4);
}

TEST(pictogram_is_drawn_inside_its_button) {
    Stage bare, car;
    bare.scene.layout.buttons[kA].glyph = Glyph::None;
    car.scene.layout.buttons[kA].glyph = Glyph::Car;
    bare.draw();
    car.draw();
    const Diff d = diff(bare.px, car.px);
    CHECK(d.count > 40);
    CHECK(insideA(d));
}

TEST(every_pictogram_draws_something_and_stays_in_its_button) {
    Stage bare;
    bare.scene.layout.buttons[kA].glyph = Glyph::None;
    bare.draw();
    for (int g = kFirstIcon; g < kFirstIcon + kIconCount; g++) {
        Stage s;
        s.scene.layout.buttons[kA].glyph = static_cast<Glyph>(g);
        s.draw();
        const Diff d = diff(bare.px, s.px);
        CHECK(d.count > 20);
        CHECK(insideA(d));
    }
}

TEST(caption_is_drawn_inside_its_button) {
    Stage bare, gas;
    bare.scene.layout.buttons[kA].glyph = Glyph::None;
    gas.scene.layout.buttons[kA].glyph = Glyph::None;
    std::snprintf(gas.scene.layout.buttons[kA].label, sizeof(ButtonDef::label), "GAS");
    bare.draw();
    gas.draw();
    const Diff d = diff(bare.px, gas.px);
    CHECK(d.count > 40);
    CHECK(insideA(d));
}

TEST(long_caption_shrinks_to_fit) {
    Stage bare, brake;
    bare.scene.layout.buttons[kA].glyph = Glyph::None;
    brake.scene.layout.buttons[kA].glyph = Glyph::None;
    std::snprintf(brake.scene.layout.buttons[kA].label, sizeof(ButtonDef::label), "HANDBRAKE");
    bare.draw();
    brake.draw();
    const Diff d = diff(bare.px, brake.px);
    CHECK(d.count > 20);
    CHECK(insideA(d));
}

TEST(caption_without_a_font_is_left_out_quietly) {
    Stage bare(false), gas(false);
    bare.scene.layout.buttons[kA].glyph = Glyph::None;
    gas.scene.layout.buttons[kA].glyph = Glyph::None;
    std::snprintf(gas.scene.layout.buttons[kA].label, sizeof(ButtonDef::label), "GAS");
    bare.draw();
    gas.draw();
    CHECK_EQ(diff(bare.px, gas.px).count, 0);
}

TEST(button_without_a_key_lights_under_the_finger) {
    Stage s;
    s.scene.layout.buttons[kA].bit = 0;
    s.draw();
    const int idle = s.alpha(601, 172);
    s.scene.visual.touched = 1u << kA;
    CHECK(s.draw());
    CHECK(s.alpha(601, 172) > idle);
    s.scene.visual.touched = 0;
    CHECK(s.draw());
    CHECK_EQ(s.alpha(601, 172), idle);
}

TEST(latched_button_stays_lit_with_no_finger_on_it) {
    Stage s;
    s.draw();
    const int idle = s.alpha(601, 172);
    s.scene.visual.pad.buttons = BtnA;  // what a toggle keeps reporting
    CHECK(s.draw());
    CHECK(s.alpha(601, 172) > idle);
}

TEST(combo_button_lights_only_when_all_its_keys_are_down) {
    Stage s;
    s.scene.layout.buttons[kA].bit = BtnA | BtnB;
    s.draw();
    const int idle = s.alpha(601, 172);
    s.scene.visual.pad.buttons = BtnB;
    s.draw();
    CHECK_EQ(s.alpha(601, 172), idle);
    s.scene.visual.pad.buttons = BtnA | BtnB;
    s.draw();
    CHECK(s.alpha(601, 172) > idle);
}

TEST(floating_stick_is_drawn_under_the_thumb_that_holds_it) {
    Stage s;
    floatLeftStick(s.scene, true);
    s.draw();
    CHECK(s.alpha(85, 150) > 0);  // resting place
    const std::vector<uint16_t> rest = s.px;

    holdLeftStick(s.scene, 400, 600);
    CHECK(s.draw());
    CHECK(s.alpha(200, 300) > 0);    // under the thumb
    CHECK_EQ(s.alpha(85, 150), 0);   // and gone from where it rested

    holdLeftStick(s.scene, 440, 560);  // the thumb overshot and dragged the centre along
    CHECK(s.draw());
    CHECK(s.alpha(220, 280) > 0);

    s.scene.visual.grabL = false;
    CHECK(s.draw());
    CHECK(s.px == rest);
}

TEST(floating_stick_without_a_hint_shows_only_while_held) {
    Stage s;
    floatLeftStick(s.scene, false);
    s.draw();
    CHECK_EQ(s.alpha(85, 150), 0);
    const std::vector<uint16_t> rest = s.px;

    holdLeftStick(s.scene, 400, 600);
    CHECK(s.draw());
    CHECK(s.alpha(200, 300) > 0);

    s.scene.visual.grabL = false;
    CHECK(s.draw());
    CHECK(s.px == rest);
    CHECK(!s.draw());  // and nothing keeps repainting afterwards
}

TEST(held_floating_stick_does_not_repaint_while_the_thumb_is_still) {
    Stage s;
    floatLeftStick(s.scene, true);
    holdLeftStick(s.scene, 400, 600);
    s.draw();
    CHECK(!s.draw());
}

TEST(editor_shows_a_hintless_floating_stick_so_it_can_be_moved) {
    Stage s;
    floatLeftStick(s.scene, false);
    s.scene.mode = Mode::Edit;
    s.draw();
    CHECK(s.alpha(85, 150) > 0);
}

TEST(moving_a_floating_stick_keeps_the_controls_it_passes_over) {
    Stage s;
    floatLeftStick(s.scene, true);
    s.draw();
    const std::vector<uint16_t> rest = s.px;
    holdLeftStick(s.scene, 150, 130);  // over ZL and L in the top-left corner
    s.draw();
    CHECK(s.alpha(46, 7) > 0);  // ZL's outline is still there
    s.scene.visual.grabL = false;
    s.draw();
    CHECK(s.px == rest);
}
