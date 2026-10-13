#include "tinytest.hpp"
#include "../core/mapper.hpp"

using namespace tp;

namespace {

// Hand-placed fixture layout with round numbers; independent of the shipped default layout.
//   A (1000,300) r50      B (920,380) r50   -> centres 113 px apart, hit areas (r+slop=64) overlap
//   X (920,220) r50       ZL round rect centre (100,60), half extents 80x40
//   left stick  (200,300) zone 140, throw 80
//   right stick (900,560) zone 120, throw 80
//   d-pad       (350,560) zone 120, dead 20
//   menu        (640,50)  r30
const Layout kLayout = {
    {
        {BtnA, Shape::Circle, 1000, 300, 50, 50, Glyph::A},
        {BtnB, Shape::Circle, 920, 380, 50, 50, Glyph::B},
        {BtnX, Shape::Circle, 920, 220, 50, 50, Glyph::X},
        {BtnZL, Shape::RoundRect, 100, 60, 80, 40, Glyph::ZL},
        {BtnL, Shape::Circle, 300, 60, 40, 40, Glyph::L},
        {BtnR, Shape::Circle, 500, 60, 40, 40, Glyph::R},
        {BtnZR, Shape::Circle, 1180, 60, 40, 40, Glyph::ZR},
        {BtnPlus, Shape::Circle, 800, 60, 30, 30, Glyph::Plus},
        {BtnMinus, Shape::Circle, 1100, 60, 30, 30, Glyph::Minus},
        {BtnY, Shape::Circle, 1180, 300, 40, 40, Glyph::Y},
    },
    10,
    {200, 300, 100, 140, 80},
    {900, 560, 100, 120, 80},
    {350, 560, 100, 36, 120, 20},
    {640, 50, 30},
    14,
};

struct Pad {
    Mapper mapper{kLayout};
    uint64_t now = 0;

    const PadState& touch(std::initializer_list<TouchPoint> touches, uint64_t advanceMs = 4) {
        now += advanceMs;
        mapper.update(touches.begin(), static_cast<int>(touches.size()), now);
        return mapper.pad();
    }
    const PadState& release(uint64_t advanceMs = 4) { return touch({}, advanceMs); }
};

bool near(int got, int want) { return got >= want - 1 && got <= want + 1; }

}  // namespace

TEST(no_touches_gives_neutral_pad) {
    Pad p;
    CHECK(p.release() == PadState{});
}

TEST(touch_on_button_presses_it) {
    Pad p;
    CHECK_EQ(p.touch({{0, 1000, 300}}).buttons, BtnA);
}

TEST(touch_inside_slop_margin_still_presses) {
    Pad p;
    // 60 px right of A's centre: outside the drawn radius (50) but inside radius + slop (64).
    CHECK_EQ(p.touch({{0, 1060, 300}}).buttons, BtnA);
}

TEST(touch_beyond_slop_margin_presses_nothing) {
    Pad p;
    // 70 px right of A's centre, clear of every other control.
    CHECK_EQ(p.touch({{0, 1070, 300}}).buttons, 0);
}

TEST(thumb_between_two_buttons_presses_both) {
    Pad p;
    // Midpoint of A and B is 56.6 px from each centre, inside both hit areas.
    CHECK_EQ(p.touch({{0, 960, 340}}).buttons, BtnA | BtnB);
}

TEST(sliding_finger_moves_from_one_button_to_another) {
    Pad p;
    CHECK_EQ(p.touch({{0, 920, 380}}).buttons, BtnB);
    CHECK_EQ(p.touch({{0, 920, 220}}).buttons, BtnX);
}

TEST(lifting_finger_releases_button) {
    Pad p;
    p.touch({{0, 1000, 300}});
    CHECK_EQ(p.release().buttons, 0);
}

TEST(round_rect_button_uses_its_half_extents) {
    Pad p;
    // ZL spans x 20..180, y 20..100; with 14 px slop the right edge reaches 194.
    CHECK_EQ(p.touch({{0, 190, 95}}).buttons, BtnZL);
    p.release();
    CHECK_EQ(p.touch({{0, 200, 60}}).buttons, 0);
}

TEST(stick_is_neutral_where_the_thumb_lands) {
    Pad p;
    // Off-centre landing inside the zone must not deflect the stick.
    const PadState& s = p.touch({{0, 230, 340}});
    CHECK_EQ(s.lx, 0);
    CHECK_EQ(s.ly, 0);
}

TEST(stick_reaches_full_deflection_at_throw_radius) {
    Pad p;
    p.touch({{0, 200, 300}});
    const PadState& s = p.touch({{0, 280, 300}});
    CHECK_EQ(s.lx, 32767);
    CHECK_EQ(s.ly, 0);
}

TEST(stick_up_is_positive_y) {
    Pad p;
    p.touch({{0, 200, 300}});
    const PadState& s = p.touch({{0, 200, 260}});  // 40 px up = half throw
    CHECK_EQ(s.lx, 0);
    CHECK(near(s.ly, 16384));
}

TEST(stick_left_and_down_are_negative) {
    Pad p;
    p.touch({{0, 200, 300}});
    const PadState& s = p.touch({{0, 160, 340}});
    CHECK(near(s.lx, -16384));
    CHECK(near(s.ly, -16384));
}

TEST(stick_never_exceeds_full_scale) {
    Pad p;
    p.touch({{0, 200, 300}});
    const PadState& s = p.touch({{0, 500, 300}});
    CHECK_EQ(s.lx, 32767);
}

TEST(stick_origin_follows_an_overshooting_thumb) {
    Pad p;
    p.touch({{0, 200, 300}});
    p.touch({{0, 400, 300}});  // 200 px right: origin is dragged along to x=320
    CHECK_EQ(p.touch({{0, 320, 300}}).lx, 0);
    CHECK(near(p.touch({{0, 280, 300}}).lx, -16384));
}

TEST(stick_ignores_jitter_inside_deadzone) {
    Pad p;
    p.touch({{0, 200, 300}});
    CHECK_EQ(p.touch({{0, 203, 300}}).lx, 0);       // 3 px < 6% of 80 px
    CHECK(near(p.touch({{0, 210, 300}}).lx, 4096)); // 10 px registers
}

TEST(stick_recentres_when_released) {
    Pad p;
    p.touch({{0, 200, 300}});
    p.touch({{0, 280, 300}});
    CHECK_EQ(p.release().lx, 0);
}

TEST(right_stick_drives_right_axes_only) {
    Pad p;
    p.touch({{0, 900, 560}});
    const PadState& s = p.touch({{0, 900, 480}});
    CHECK_EQ(s.ry, 32767);
    CHECK_EQ(s.lx, 0);
    CHECK_EQ(s.ly, 0);
}

TEST(stick_finger_does_not_press_buttons_it_slides_over) {
    Pad p;
    p.touch({{0, 900, 560}});                      // grabs the right stick
    const PadState& s = p.touch({{0, 920, 380}});  // now directly over B
    CHECK_EQ(s.buttons, 0);
    CHECK(s.ry > 30000);  // still the stick's finger, pushed (almost straight) up
}

TEST(finger_sliding_into_stick_zone_does_not_grab_it) {
    Pad p;
    p.touch({{0, 920, 380}});                      // starts on B
    const PadState& s = p.touch({{0, 900, 560}});  // slides onto the right stick
    CHECK_EQ(s.rx, 0);
    CHECK_EQ(s.ry, 0);
    p.touch({{0, 980, 560}});
    CHECK_EQ(p.mapper.pad().rx, 0);
}

TEST(second_finger_in_stick_zone_does_not_disturb_the_stick) {
    Pad p;
    p.touch({{0, 200, 300}});
    p.touch({{0, 280, 300}});
    const PadState& s = p.touch({{0, 280, 300}, {1, 150, 250}});
    CHECK_EQ(s.lx, 32767);
    CHECK_EQ(s.ly, 0);
}

TEST(dpad_cardinal_directions) {
    Pad p;
    CHECK_EQ(p.touch({{0, 420, 560}}).buttons, BtnRight);
    p.release();
    CHECK_EQ(p.touch({{0, 280, 560}}).buttons, BtnLeft);
    p.release();
    CHECK_EQ(p.touch({{0, 350, 490}}).buttons, BtnUp);
    p.release();
    CHECK_EQ(p.touch({{0, 350, 630}}).buttons, BtnDown);
}

TEST(dpad_diagonal_presses_two_directions) {
    Pad p;
    CHECK_EQ(p.touch({{0, 420, 490}}).buttons, BtnUp | BtnRight);
    p.release();
    CHECK_EQ(p.touch({{0, 290, 620}}).buttons, BtnDown | BtnLeft);
}

TEST(dpad_slightly_off_axis_is_still_cardinal) {
    Pad p;
    // 70 right, 20 up: 16 degrees off the axis, inside the 22.5 degree cardinal sector.
    CHECK_EQ(p.touch({{0, 420, 540}}).buttons, BtnRight);
}

TEST(dpad_centre_is_dead) {
    Pad p;
    CHECK_EQ(p.touch({{0, 355, 563}}).buttons, 0);
}

TEST(dpad_thumb_can_roll_between_directions) {
    Pad p;
    p.touch({{0, 420, 560}});
    CHECK_EQ(p.touch({{0, 350, 490}}).buttons, BtnUp);
}

TEST(dpad_keeps_direction_when_thumb_drifts_out_of_zone) {
    Pad p;
    p.touch({{0, 420, 560}});
    CHECK_EQ(p.touch({{0, 700, 560}}).buttons, BtnRight);
}

TEST(stick_and_buttons_work_together) {
    Pad p;
    p.touch({{0, 200, 300}});
    const PadState& s = p.touch({{0, 280, 300}, {1, 1000, 300}});
    CHECK_EQ(s.lx, 32767);
    CHECK_EQ(s.buttons, BtnA);
}

TEST(ten_fingers_all_register) {
    Pad p;
    p.touch({{0, 200, 300}, {1, 900, 560}, {2, 420, 560}});
    const PadState& s = p.touch({
        {0, 280, 300},   // left stick right
        {1, 900, 480},   // right stick up
        {2, 420, 560},   // d-pad right
        {3, 1000, 300},  // A
        {4, 920, 220},   // X
        {5, 100, 60},    // ZL
        {6, 300, 60},    // L
        {7, 500, 60},    // R
        {8, 1180, 60},   // ZR
        {9, 800, 60},    // Plus
    });
    CHECK_EQ(s.lx, 32767);
    CHECK_EQ(s.ry, 32767);
    CHECK_EQ(s.buttons, BtnRight | BtnA | BtnX | BtnZL | BtnL | BtnR | BtnZR | BtnPlus);
}

TEST(menu_tap_is_reported_once_when_the_finger_lifts) {
    Pad p;
    p.touch({{0, 640, 50}});
    CHECK(!p.mapper.takeMenuTap());  // nothing happens until the finger lifts
    p.release(100);
    CHECK(p.mapper.takeMenuTap());
    CHECK(!p.mapper.takeMenuTap());
}

TEST(paused_pad_ignores_controls) {
    Pad p;
    p.mapper.setPaused(true);
    CHECK_EQ(p.touch({{0, 1000, 300}}).buttons, 0);
    p.release();
    p.touch({{0, 200, 300}});
    CHECK_EQ(p.touch({{0, 280, 300}}).lx, 0);
}

TEST(paused_pad_still_reports_menu_taps) {
    Pad p;
    p.mapper.setPaused(true);
    p.touch({{0, 640, 50}});
    p.release(100);
    CHECK(p.mapper.takeMenuTap());
}

TEST(pausing_drops_inputs_that_are_still_held) {
    Pad p;
    p.touch({{0, 1000, 300}});
    p.mapper.setPaused(true);
    CHECK_EQ(p.touch({{0, 1000, 300}}).buttons, 0);
}

TEST(finger_held_through_resume_stays_inert_until_lifted) {
    Pad p;
    p.mapper.setPaused(true);
    p.touch({{0, 1000, 300}});  // lands on A while paused
    p.mapper.setPaused(false);
    CHECK_EQ(p.touch({{0, 1000, 300}}).buttons, 0);  // never lifted
    p.release();
    CHECK_EQ(p.touch({{0, 1000, 300}}).buttons, BtnA);
}

TEST(reset_forgets_every_finger) {
    Pad p;
    p.touch({{0, 200, 300}});
    p.touch({{0, 280, 300}});
    p.mapper.reset();
    CHECK(p.mapper.pad() == PadState{});
    // The same finger is still down but must not act until it lifts and lands again.
    CHECK_EQ(p.touch({{0, 280, 300}}).lx, 0);
}

TEST(holding_menu_requests_exit) {
    Pad p;
    p.touch({{0, 640, 50}});
    p.touch({{0, 640, 50}}, 700);
    CHECK(!p.mapper.exitRequested());
    p.touch({{0, 640, 50}}, 200);  // 900 ms held in total, past the 800 ms threshold
    CHECK(p.mapper.exitRequested());
}

TEST(short_menu_tap_does_not_request_exit) {
    Pad p;
    p.touch({{0, 640, 50}});
    p.release(300);
    CHECK(!p.mapper.exitRequested());
}

TEST(menu_hold_reports_progress_for_feedback) {
    Pad p;
    p.touch({{0, 640, 50}});
    CHECK_EQ(p.mapper.visual().menuHold, 1);
    p.touch({{0, 640, 50}}, 400);  // half of 800 ms
    CHECK_EQ(p.mapper.visual().menuHold, 2);
    p.release();
    CHECK_EQ(p.mapper.visual().menuHold, 0);
}

TEST(sliding_off_menu_cancels_tap_and_hold) {
    Pad p;
    p.touch({{0, 640, 50}});
    p.touch({{0, 640, 300}}, 100);
    p.touch({{0, 640, 300}}, 1000);
    CHECK(!p.mapper.exitRequested());
    p.release();
    CHECK(!p.mapper.takeMenuTap());
}

TEST(menu_finger_presses_no_gamepad_buttons) {
    Pad p;
    p.touch({{0, 640, 50}});
    CHECK_EQ(p.touch({{0, 800, 60}}, 20).buttons, 0);  // slid onto Plus
}

namespace {

Layout withHidden() {
    Layout l = kLayout;
    l.buttons[0].visible = false;  // A
    l.stickL.visible = false;
    l.dpad.visible = false;
    return l;
}

}  // namespace

TEST(hidden_button_cannot_be_pressed) {
    const Layout l = withHidden();
    Mapper m(l);
    const TouchPoint a{0, 1000, 300};
    m.update(&a, 1, 4);
    CHECK_EQ(m.pad().buttons, 0);
    const TouchPoint between{1, 960, 340};  // used to press A and B together
    m.update(&between, 1, 8);
    CHECK_EQ(m.pad().buttons, BtnB);
}

TEST(hidden_stick_does_not_grab_touches) {
    const Layout l = withHidden();
    Mapper m(l);
    TouchPoint t{0, 200, 300};
    m.update(&t, 1, 4);
    t.x = 280;
    m.update(&t, 1, 8);
    CHECK_EQ(m.pad().lx, 0);
}

TEST(hidden_dpad_presses_nothing) {
    const Layout l = withHidden();
    Mapper m(l);
    const TouchPoint t{0, 420, 560};
    m.update(&t, 1, 4);
    CHECK_EQ(m.pad().buttons, 0);
}

// ---- controls made for a game: latches, turbo, drag buttons, floating sticks ----

namespace {

// A layout the way a game pack builds one.
//   0 gas     A  toggle        (1000,600) r50
//   1 sprint  B  turbo         (850,600)  r50
//   2 fire    ZR drags the right stick   (1150,450) r60
//   3 enter   Y  switches to layout 1    (1150,250) r45
//   4 pill    no key, goes back          (760,44) 46x28
//   5 both    ZL+ZR together             (640,300) 60x40
//   left stick floats in the left half below y=200, right stick floats on the right without a hint
Layout gameLayout() {
    Layout l = defaultLayout();
    l.buttonCount = 0;
    auto add = [&l](uint32_t bit, Shape shape, int x, int y, int rx, int ry) -> ButtonDef& {
        ButtonDef b{};
        b.bit = bit;
        b.shape = shape;
        b.cx = static_cast<int16_t>(x);
        b.cy = static_cast<int16_t>(y);
        b.rx = static_cast<int16_t>(rx);
        b.ry = static_cast<int16_t>(ry);
        l.buttons[l.buttonCount] = b;
        return l.buttons[l.buttonCount++];
    };
    add(BtnA, Shape::Circle, 1000, 600, 50, 50).press = Press::Toggle;
    add(BtnB, Shape::Circle, 850, 600, 50, 50).press = Press::Turbo;
    add(BtnZR, Shape::Circle, 1150, 450, 60, 60).drag = Drag::StickR;
    add(BtnY, Shape::Circle, 1150, 250, 45, 45).gotoLayout = 1;
    add(0, Shape::RoundRect, 760, 44, 46, 28).gotoLayout = kGotoBack;
    add(BtnZL | BtnZR, Shape::RoundRect, 640, 300, 60, 40);

    l.dpad.visible = false;
    l.stickL = StickDef{200, 500, 100, 135, 80, true, true, true, 0, 200, 560, 720};
    l.stickR = StickDef{900, 330, 90, 125, 80, true, true, false, 580, 100, 1280, 720};
    return l;
}

struct GamePad {
    Layout layout = gameLayout();
    Mapper mapper{layout};
    uint64_t now = 0;

    const PadState& touch(std::initializer_list<TouchPoint> touches, uint64_t advanceMs = 4) {
        now += advanceMs;
        mapper.update(touches.begin(), static_cast<int>(touches.size()), now);
        return mapper.pad();
    }
    const PadState& release(uint64_t advanceMs = 4) { return touch({}, advanceMs); }
};

constexpr TouchPoint kGas{1, 1000, 600};
constexpr TouchPoint kSprint{1, 850, 600};
constexpr TouchPoint kFire{1, 1150, 450};
constexpr TouchPoint kEnter{1, 1150, 250};

}  // namespace

TEST(toggle_button_stays_down_after_the_finger_leaves) {
    GamePad p;
    CHECK_EQ(p.touch({kGas}).buttons, BtnA);
    CHECK_EQ(p.release().buttons, BtnA);
    CHECK_EQ(p.release(500).buttons, BtnA);
}

TEST(toggle_button_comes_up_on_the_next_touch) {
    GamePad p;
    p.touch({kGas});
    p.release();
    CHECK_EQ(p.touch({kGas}).buttons, 0);
    CHECK_EQ(p.touch({kGas}, 300).buttons, 0);  // resting on it changes nothing
    CHECK_EQ(p.release().buttons, 0);
}

TEST(a_latched_toggle_leaves_the_thumb_free_for_other_buttons) {
    GamePad p;
    p.touch({kGas});
    p.release();
    CHECK_EQ(p.touch({kFire}).buttons, BtnA | BtnZR);
    CHECK_EQ(p.release().buttons, BtnA);
}

TEST(turbo_button_taps_by_itself_while_held) {
    GamePad p;
    int presses = 0;
    bool was = false;
    for (int ms = 0; ms < 900; ms += 4) {
        const bool down = p.touch({kSprint}).buttons & BtnB;
        if (down && !was) presses++;
        was = down;
    }
    CHECK_EQ(presses, 5);  // 90 ms down, 90 ms up
    CHECK_EQ(p.release().buttons, 0);
}

TEST(turbo_button_is_down_the_moment_it_is_touched) {
    GamePad p;
    CHECK_EQ(p.touch({kSprint}).buttons, BtnB);
}

TEST(goto_button_presses_its_key_and_asks_for_the_layout_on_release) {
    GamePad p;
    CHECK_EQ(p.touch({kEnter}).buttons, BtnY);
    CHECK_EQ(p.mapper.takeGoto(), kGotoNone);  // the game gets the whole press first
    p.touch({kEnter}, 200);
    CHECK_EQ(p.mapper.takeGoto(), kGotoNone);
    p.release();
    CHECK_EQ(p.mapper.takeGoto(), 1);
    CHECK_EQ(p.mapper.takeGoto(), kGotoNone);  // reported once
}

TEST(button_without_a_key_presses_nothing_but_still_switches) {
    GamePad p;
    CHECK(p.touch({{1, 760, 44}}) == PadState{});
    CHECK_EQ(p.mapper.visual().touched, 1u << 4);  // it still lights up
    p.release();
    CHECK_EQ(p.mapper.takeGoto(), kGotoBack);
}

TEST(pausing_under_a_goto_button_does_not_switch) {
    GamePad p;
    p.touch({kEnter});
    p.mapper.setPaused(true);
    p.touch({kEnter});
    p.release();
    CHECK_EQ(p.mapper.takeGoto(), kGotoNone);
}

TEST(combo_button_presses_all_its_keys) {
    GamePad p;
    CHECK_EQ(p.touch({{1, 640, 300}}).buttons, BtnZL | BtnZR);
}

TEST(drag_button_holds_its_key_and_steers_the_stick_with_the_same_finger) {
    GamePad p;
    CHECK_EQ(p.touch({kFire}).buttons, BtnZR);
    CHECK_EQ(p.mapper.pad().rx, 0);
    const PadState& s = p.touch({{1, kFire.x + 90, kFire.y}});
    CHECK_EQ(s.buttons, BtnZR);
    CHECK(near(s.rx, kStickMax));
    CHECK_EQ(s.ry, 0);
    // Far outside the button it is still the same press: the finger belongs to it now.
    const PadState& far = p.touch({{1, kFire.x - 200, kFire.y + 40}});
    CHECK_EQ(far.buttons, BtnZR);
    CHECK(far.rx < -kStickMax / 2);
    CHECK(p.release() == PadState{});
}

TEST(drag_button_finger_does_not_spill_onto_neighbours) {
    GamePad p;
    p.touch({kFire});
    CHECK_EQ(p.touch({{1, kGas.x, kGas.y}}).buttons, BtnZR);  // slid over the gas: still only fire
    p.release();
    CHECK_EQ(p.mapper.pad().buttons, 0);  // and the gas was not latched on the way
}

TEST(thumb_on_the_stick_outranks_a_drag_button) {
    GamePad p;
    p.touch({{1, 800, 300}});                             // right stick grabbed in its zone
    p.touch({{1, 880, 300}});                             // pushed fully right
    const PadState& s = p.touch({{1, 880, 300}, {2, kFire.x, kFire.y}});
    CHECK_EQ(s.buttons, BtnZR);
    CHECK(near(s.rx, kStickMax));
    const PadState& t = p.touch({{1, 880, 300}, {2, kFire.x - 90, kFire.y}});
    CHECK(near(t.rx, kStickMax));  // the fire finger moving left changes nothing
}

TEST(floating_stick_starts_wherever_the_thumb_lands_in_its_zone) {
    GamePad p;
    CHECK(p.touch({{1, 420, 650}}) == PadState{});  // far from where the stick rests
    const Visual v = p.mapper.visual();
    CHECK(v.grabL);
    CHECK_EQ(v.lox, 420);
    CHECK_EQ(v.loy, 650);
    const PadState& s = p.touch({{1, 420, 570}});
    CHECK_EQ(s.lx, 0);
    CHECK(near(s.ly, kStickMax));
}

TEST(floating_stick_ignores_touches_outside_its_zone) {
    GamePad p;
    p.touch({{1, 300, 120}});  // above the left zone
    CHECK(p.touch({{1, 380, 120}}) == PadState{});
    CHECK(!p.mapper.visual().grabL);
}

TEST(floating_stick_stays_where_the_thumb_landed_however_far_it_goes) {
    GamePad p;
    p.touch({{1, 300, 500}});
    p.touch({{1, 500, 500}});  // 200 px right, throw is 80
    const Visual v = p.mapper.visual();
    CHECK_EQ(v.lox, 300);  // it does not chase the thumb across the screen
    CHECK(near(v.pad.lx, kStickMax));
    CHECK(p.touch({{1, 340, 500}}).lx > 0);  // back inside the throw: half way, not a reversal
}

TEST(buttons_inside_a_stick_zone_still_win) {
    GamePad p;
    CHECK_EQ(p.touch({kGas}).buttons, BtnA);  // the gas sits inside the right stick's zone
    CHECK(!p.mapper.visual().grabR);
}

TEST(released_stick_is_no_longer_reported_as_held) {
    GamePad p;
    p.touch({{1, 300, 500}});
    p.release();
    CHECK(!p.mapper.visual().grabL);
}

TEST(relayout_keeps_the_steering_thumb_and_drops_the_rest) {
    GamePad p;
    p.touch({{1, 300, 500}});
    p.touch({{1, 380, 500}, {2, kGas.x, kGas.y}});
    CHECK_EQ(p.mapper.pad().buttons, BtnA);

    p.layout.buttons[0].bit = BtnX;  // the layout changed under the fingers
    p.mapper.relayout();
    const PadState& s = p.touch({{1, 380, 500}, {2, kGas.x, kGas.y}});
    CHECK(near(s.lx, kStickMax));  // still steering
    CHECK_EQ(s.buttons, 0);        // the other finger must lift first, and the latch is gone
    p.touch({{1, 380, 500}});
    CHECK_EQ(p.touch({{1, 380, 500}, {3, kGas.x, kGas.y}}).buttons, BtnX);
}

TEST(relayout_lets_go_of_a_stick_the_new_layout_does_not_have) {
    GamePad p;
    p.touch({{1, 300, 500}});
    p.touch({{1, 380, 500}});
    p.layout.stickL.visible = false;
    p.mapper.relayout();
    CHECK_EQ(p.touch({{1, 380, 500}}).lx, 0);
}

TEST(relayout_forgets_a_pending_switch) {
    GamePad p;
    p.touch({kEnter});
    p.release();
    p.mapper.relayout();
    CHECK_EQ(p.mapper.takeGoto(), kGotoNone);
}
