#include "tinytest.hpp"
#include "../core/app.hpp"
#include "roam_pack.hpp"

#include <cstring>
#include <string>

using namespace tp;

namespace {

constexpr uint64_t kMarioKart = 0x0100152000022000ull;
constexpr uint64_t kSomeGame = 0x0100AAAA00001000ull;  // no pack is made for it
constexpr uint64_t kOtherGame = 0x01007EF00011E000ull;

// Default layout landmarks (see core/layout.cpp).
constexpr TouchPoint kOnA{0, 1202, 300};
constexpr TouchPoint kOnMenuButton{0, 640, 46};
constexpr TouchPoint kRightStick{0, 950, 565};
constexpr TouchPoint kBelowMenu{0, 640, 660};   // under the menu panel, clear of controls
constexpr TouchPoint kEmptySpot{0, 640, 250};   // middle of the screen: no control, and above the editor toolbar

int midX(const Rect& r) { return (r.x0 + r.x1) / 2; }
int midY(const Rect& r) { return (r.y0 + r.y1) / 2; }

struct Rig {
    App app;
    uint64_t now = 1000;

    Rig() { app.setGame(0); }

    const PadState& touch(std::initializer_list<TouchPoint> touches, uint64_t advanceMs = 8) {
        now += advanceMs;
        app.update(touches.begin(), static_cast<int>(touches.size()), now);
        return app.pad();
    }
    void lift() { touch({}); }
    void tap(int x, int y) {
        touch({{0, x, y}});
        touch({{0, x, y}}, 40);
        lift();
    }
    void tap(const Rect& r) { tap(midX(r), midY(r)); }
    void drag(int x0, int y0, int x1, int y1) {
        touch({{0, x0, y0}});
        for (int i = 1; i <= 10; i++) touch({{0, x0 + (x1 - x0) * i / 10, y0 + (y1 - y0) * i / 10}});
        lift();
    }
    void openMenu() { tap(kOnMenuButton.x, kOnMenuButton.y); }
    void openEditor() {
        openMenu();
        tap(ui::menuRow(RowEdit));
    }
    Scene scene() const {
        Scene s;
        app.fillScene(s);
        return s;
    }
    const ButtonDef& button(Button bit) const {
        static ButtonDef none{};
        const Scene s = scene();
        static Scene keep;
        keep = s;
        for (int i = 0; i < keep.layout.buttonCount; i++)
            if (keep.layout.buttons[i].bit == bit) return keep.layout.buttons[i];
        return none;
    }
    std::string packId() { return app.packs().items[app.currentPack()].id; }
};

}  // namespace

// ---- playing ----

TEST(app_starts_playing_with_the_full_pack) {
    Rig r;
    CHECK(r.app.mode() == Mode::Play);
    CHECK(r.packId() == "full");
    CHECK_EQ(r.touch({kOnA}).buttons, BtnA);
    CHECK(r.app.blockGameTouch());
}

TEST(holding_the_menu_button_asks_to_leave_for_ultrahand) {
    Rig r;
    r.touch({kOnMenuButton});
    r.touch({kOnMenuButton}, 900);
    CHECK(r.app.takeExitRequest());
    CHECK(!r.app.takeExitRequest());
}

// ---- menu ----

TEST(tapping_the_menu_button_opens_the_menu_and_freezes_the_pad) {
    Rig r;
    r.openMenu();
    CHECK(r.app.mode() == Mode::Menu);
    CHECK_EQ(r.touch({kOnA}).buttons, 0);
    CHECK(r.app.blockGameTouch());  // taps on the menu must not reach the game
}

TEST(tapping_the_menu_button_again_closes_the_menu) {
    Rig r;
    r.openMenu();
    r.openMenu();
    CHECK(r.app.mode() == Mode::Play);
    CHECK_EQ(r.touch({kOnA}).buttons, BtnA);
}

TEST(tapping_outside_the_panel_closes_the_menu) {
    Rig r;
    r.openMenu();
    r.tap(kBelowMenu.x, kBelowMenu.y);
    CHECK(r.app.mode() == Mode::Play);
}

TEST(hide_row_hides_the_pad_and_lets_touches_through) {
    Rig r;
    r.openMenu();
    r.tap(ui::menuRow(RowHide));
    CHECK(r.app.mode() == Mode::Hidden);
    CHECK(!r.app.blockGameTouch());
    CHECK_EQ(r.touch({kOnA}).buttons, 0);
    r.lift();
    r.openMenu();  // one tap on the menu button brings the pad straight back
    CHECK(r.app.mode() == Mode::Play);
}

TEST(pack_arrow_switches_to_the_next_pack) {
    Rig r;
    r.openMenu();
    r.tap(ui::menuArrow(RowPack, true));
    CHECK(r.packId() == "platformer");
    CHECK(std::string(r.scene().packName) == "Platformer");
    CHECK(r.app.mode() == Mode::Menu);  // stays open so you can keep flipping
    r.openMenu();
    // Racing has no right stick: its area is dead now.
    r.touch({kRightStick});
    CHECK_EQ(r.touch({{0, kRightStick.x + 80, kRightStick.y}}).rx, 0);
}

TEST(pack_left_arrow_wraps_to_the_last_pack) {
    Rig r;
    r.openMenu();
    r.tap(ui::menuArrow(RowPack, false));
    CHECK_EQ(r.app.currentPack(), r.app.packs().count - 1);
}

TEST(tapping_the_pack_row_itself_also_advances) {
    Rig r;
    r.openMenu();
    r.tap(ui::menuRow(RowPack).x0 + 60, midY(ui::menuRow(RowPack)));
    CHECK(r.packId() == "platformer");
}

TEST(pack_choice_is_remembered_for_the_running_game) {
    Rig r;
    r.app.setGame(kSomeGame);
    r.openMenu();
    r.tap(ui::menuArrow(RowPack, true));
    CHECK(r.app.takeSettingsDirty());
    CHECK(!r.app.takeSettingsDirty());
    const char* bound = r.app.settings().packFor(kSomeGame);
    CHECK(bound && std::string(bound) == "platformer");
    CHECK(std::string(r.app.settings().defaultPack) == "full");  // other games are unaffected
}

TEST(pack_choice_without_a_game_becomes_the_default) {
    Rig r;
    r.openMenu();
    r.tap(ui::menuArrow(RowPack, true));
    CHECK(std::string(r.app.settings().defaultPack) == "platformer");
    CHECK_EQ(r.app.settings().gameCount, 0);
}

TEST(each_game_gets_its_own_pack_back) {
    Rig r;
    r.app.settings().bind(kSomeGame, "platformer");
    r.app.settings().bind(kOtherGame, "dpad");
    r.app.setGame(kSomeGame);
    CHECK(r.packId() == "platformer");
    r.app.setGame(kOtherGame);
    CHECK(r.packId() == "dpad");
    r.app.setGame(0x1234);  // never seen: default pack
    CHECK(r.packId() == "full");
    CHECK(!r.app.takeSettingsDirty());  // merely following the game changes nothing on disk
}

TEST(game_bound_to_a_deleted_pack_falls_back_to_the_first_one) {
    Rig r;
    r.app.settings().bind(kSomeGame, "gone");
    r.app.setGame(kSomeGame);
    CHECK_EQ(r.app.currentPack(), 0);
}

TEST(colour_row_cycles_through_presets) {
    Rig r;
    r.openMenu();
    r.tap(ui::menuArrow(RowColor, true));
    Theme want;
    want.r = kColorPresets[1].r;
    want.g = kColorPresets[1].g;
    want.b = kColorPresets[1].b;
    CHECK(r.app.settings().theme == want);
    CHECK(std::string(r.scene().rowValue[RowColor]) == kColorPresets[1].name);
    CHECK(r.app.takeSettingsDirty());
    r.tap(ui::menuArrow(RowColor, false));
    r.tap(ui::menuArrow(RowColor, false));
    CHECK_EQ(presetIndex(r.app.settings().theme), kColorPresetCount - 1);
}

TEST(hand_picked_colour_shows_as_custom_and_cycling_leaves_it) {
    Rig r;
    r.app.settings().theme.r = 1;
    r.app.settings().theme.g = 2;
    r.app.settings().theme.b = 3;
    r.openMenu();
    CHECK(std::string(r.scene().rowValue[RowColor]) == "Custom");
    r.tap(ui::menuArrow(RowColor, true));
    CHECK_EQ(presetIndex(r.app.settings().theme), 0);
}

TEST(opacity_arrows_stop_at_the_limits) {
    Rig r;
    r.openMenu();
    r.tap(ui::menuArrow(RowOpacity, true));
    CHECK_EQ(r.app.settings().theme.opacity, 5);
    r.tap(ui::menuArrow(RowOpacity, true));
    CHECK_EQ(r.app.settings().theme.opacity, 5);
    for (int i = 0; i < 8; i++) r.tap(ui::menuArrow(RowOpacity, false));
    CHECK_EQ(r.app.settings().theme.opacity, 1);
    CHECK(std::string(r.scene().rowValue[RowOpacity]) == "1 / 5");
}

TEST(face_colours_row_toggles) {
    Rig r;
    r.openMenu();
    CHECK(std::string(r.scene().rowValue[RowFaceColors]) == "off");
    r.tap(ui::menuRow(RowFaceColors));
    CHECK(r.app.settings().theme.faceColors);
    CHECK(std::string(r.scene().rowValue[RowFaceColors]) == "on");
    r.tap(ui::menuRow(RowFaceColors));
    CHECK(!r.app.settings().theme.faceColors);
}

TEST(ultrahand_row_asks_to_leave) {
    Rig r;
    r.openMenu();
    r.tap(ui::menuRow(RowUltrahand));
    CHECK(r.app.takeExitRequest());
}

TEST(sliding_off_a_row_cancels_it) {
    Rig r;
    r.openMenu();
    const Rect row = ui::menuRow(RowHide);
    r.touch({{0, midX(row), midY(row)}});
    r.touch({{0, midX(row), row.y1 + 150}});
    r.lift();
    CHECK(r.app.mode() == Mode::Menu);
}

TEST(pressed_row_is_reported_so_it_can_be_highlighted) {
    Rig r;
    r.openMenu();
    const uint32_t before = r.app.revision();
    const Rect arrow = ui::menuArrow(RowColor, true);
    r.touch({{0, midX(arrow), midY(arrow)}});
    CHECK_EQ(r.scene().pressedRow, RowColor);
    CHECK_EQ(r.scene().pressedArrow, 1);
    CHECK(r.app.revision() != before);
    r.lift();
    CHECK_EQ(r.scene().pressedRow, -1);
}

TEST(finger_already_down_when_the_menu_opens_cannot_pick_a_row) {
    Rig r;
    const Rect row = ui::menuRow(RowUltrahand);
    const TouchPoint resting{5, midX(row), midY(row)};  // thumb resting where the row will appear
    r.touch({resting});
    r.touch({resting, {6, kOnMenuButton.x, kOnMenuButton.y}});
    r.touch({resting}, 40);  // menu button tapped: menu opens under the resting thumb
    CHECK(r.app.mode() == Mode::Menu);
    r.lift();
    CHECK(!r.app.takeExitRequest());
}

TEST(scene_carries_what_the_menu_shows) {
    Rig r;
    r.openMenu();
    const Scene s = r.scene();
    CHECK(s.mode == Mode::Menu);
    CHECK(std::string(s.rowValue[RowPack]) == "All buttons");
    CHECK(std::string(s.rowValue[RowColor]) == "White");
    CHECK(std::string(s.rowValue[RowOpacity]) == "4 / 5");
    CHECK(std::string(ui::rowLabel(RowUltrahand)) == "Exit to Ultrahand");
    CHECK_EQ(s.layout.buttonCount, 14);
}

// ---- layout editor ----

TEST(edit_row_opens_the_editor) {
    Rig r;
    r.openEditor();
    CHECK(r.app.mode() == Mode::Edit);
    CHECK_EQ(r.touch({kOnA}).buttons, 0);
}

TEST(dragging_a_button_moves_it_and_saving_keeps_it) {
    Rig r;
    r.openEditor();
    r.drag(1202, 300, 1002, 500);
    CHECK_EQ(r.button(BtnA).cx, 1002);
    CHECK_EQ(r.button(BtnA).cy, 500);
    r.tap(ui::toolButton(ToolSave));
    CHECK(r.app.mode() == Mode::Play);
    CHECK_EQ(r.app.takePackToSave(), 0);
    CHECK_EQ(r.app.takePackToSave(), -1);
    CHECK_EQ(r.app.packs().items[0].layouts[0].layout.buttons[4].cx, 1002);  // A is the fifth button
    CHECK_EQ(r.touch({{0, 1002, 500}}).buttons, BtnA);
    r.lift();
    CHECK_EQ(r.touch({kOnA}).buttons, 0);
}

TEST(dragging_keeps_the_grab_point_under_the_finger) {
    Rig r;
    r.openEditor();
    r.drag(1222, 310, 1022, 510);  // grabbed 20 right and 10 below the centre
    CHECK_EQ(r.button(BtnA).cx, 1002);
    CHECK_EQ(r.button(BtnA).cy, 500);
}

TEST(dragging_cannot_push_a_control_off_the_screen) {
    Rig r;
    r.openEditor();
    r.drag(1202, 300, 1500, -200);
    const ButtonDef& a = r.button(BtnA);
    CHECK_EQ(a.cx, kScreenW - a.rx);
    CHECK_EQ(a.cy, a.ry);
}

TEST(sticks_and_the_dpad_can_be_dragged_too) {
    Rig r;
    r.openEditor();
    r.drag(170, 300, 200, 470);
    r.drag(330, 565, 520, 600);
    const Scene s = r.scene();
    CHECK_EQ(s.layout.stickL.cx, 200);
    CHECK_EQ(s.layout.stickL.cy, 470);
    CHECK_EQ(s.layout.dpad.cx, 520);
    CHECK_EQ(s.layout.dpad.cy, 600);
}

TEST(tapping_a_control_selects_it_and_size_tools_resize_it) {
    Rig r;
    r.openEditor();
    r.tap(kOnA.x, kOnA.y);
    CHECK(r.scene().selected.kind == ControlRef::ButtonC);
    r.tap(ui::toolButton(ToolBigger));
    CHECK_EQ(r.button(BtnA).rx, 56);
    CHECK_EQ(r.button(BtnA).ry, 56);
    for (int i = 0; i < 40; i++) r.tap(ui::toolButton(ToolSmaller));
    CHECK_EQ(r.button(BtnA).rx, 24);  // lower limit
    r.tap(170, 300);
    r.tap(ui::toolButton(ToolBigger));
    CHECK_EQ(r.scene().layout.stickL.baseRadius, 111);
    CHECK_EQ(r.scene().layout.stickL.zoneRadius, 146);
}

TEST(size_tools_do_nothing_without_a_selection) {
    Rig r;
    r.openEditor();
    const Scene before = r.scene();
    r.tap(ui::toolButton(ToolBigger));
    r.tap(ui::toolButton(ToolShow));
    const Scene after = r.scene();
    CHECK(before.layout == after.layout);
}

TEST(tapping_empty_space_clears_the_selection) {
    Rig r;
    r.openEditor();
    r.tap(kOnA.x, kOnA.y);
    r.tap(kEmptySpot.x, kEmptySpot.y);
    CHECK(r.scene().selected.kind == ControlRef::None);
}

TEST(hidden_control_stays_selectable_in_the_editor_but_is_gone_in_play) {
    Rig r;
    r.openEditor();
    r.tap(kOnA.x, kOnA.y);
    r.tap(ui::toolButton(ToolShow));
    CHECK(!r.button(BtnA).visible);
    CHECK(std::string(ui::toolLabel(ToolShow, false)) == "Show");
    r.tap(kEmptySpot.x, kEmptySpot.y);
    r.tap(kOnA.x, kOnA.y);  // still there to be picked and shown again
    CHECK(r.scene().selected.kind == ControlRef::ButtonC);
    r.tap(ui::toolButton(ToolSave));
    CHECK_EQ(r.touch({kOnA}).buttons, 0);
}

TEST(cancel_puts_everything_back) {
    Rig r;
    r.openEditor();
    r.drag(1202, 300, 1002, 500);
    r.tap(ui::toolButton(ToolCancel));
    CHECK(r.app.mode() == Mode::Play);
    CHECK_EQ(r.app.takePackToSave(), -1);
    CHECK_EQ(r.touch({kOnA}).buttons, BtnA);
}

TEST(menu_button_in_the_editor_also_cancels) {
    Rig r;
    r.openEditor();
    r.drag(1202, 300, 1002, 500);
    r.openMenu();
    CHECK(r.app.mode() == Mode::Play);
    CHECK_EQ(r.button(BtnA).cx, 1202);
}

TEST(save_as_new_creates_a_pack_and_leaves_the_original_alone) {
    Rig r;
    r.app.setGame(kSomeGame);
    const int before = r.app.packs().count;
    r.openEditor();
    r.drag(1202, 300, 1002, 500);
    r.tap(ui::toolButton(ToolSaveNew));
    CHECK_EQ(r.app.packs().count, before + 1);
    CHECK_EQ(r.app.currentPack(), before);
    CHECK(r.packId() == "custom1");
    CHECK(std::string(r.scene().packName) == "Custom 1");
    CHECK_EQ(r.app.takePackToSave(), before);
    CHECK(std::string(r.app.settings().packFor(kSomeGame)) == "custom1");
    CHECK(r.app.takeSettingsDirty());
    CHECK_EQ(r.app.packs().items[0].layouts[0].layout.buttons[4].cx, 1202);  // "full" untouched
}

TEST(second_new_pack_gets_the_next_free_name) {
    Rig r;
    r.openEditor();
    r.tap(ui::toolButton(ToolSaveNew));
    r.openEditor();
    r.tap(ui::toolButton(ToolSaveNew));
    CHECK(r.packId() == "custom2");
}

TEST(save_as_new_with_no_room_left_saves_over_the_current_pack) {
    Rig r;
    while (r.app.packs().count < kMaxPacks) {
        static Pack p;
        p = r.app.packs().items[0];
        std::snprintf(p.id, sizeof(p.id), "x%d", r.app.packs().count);
        r.app.packs().put(p);
    }
    r.openEditor();
    r.drag(1202, 300, 1002, 500);
    r.tap(ui::toolButton(ToolSaveNew));
    CHECK_EQ(r.app.packs().count, kMaxPacks);
    CHECK_EQ(r.app.takePackToSave(), 0);
    CHECK_EQ(r.app.packs().items[0].layouts[0].layout.buttons[4].cx, 1002);
}

TEST(game_change_during_editing_does_not_swap_the_layout_under_the_finger) {
    Rig r;
    r.app.settings().bind(kSomeGame, "platformer");
    r.openEditor();
    r.drag(1202, 300, 1002, 500);
    r.app.setGame(kSomeGame);
    CHECK(r.app.mode() == Mode::Edit);
    CHECK_EQ(r.button(BtnA).cx, 1002);
}

TEST(visual_marks_the_pad_as_hidden_only_in_hidden_mode) {
    Rig r;
    CHECK(!r.app.visual().paused);
    r.openMenu();
    CHECK(!r.app.visual().paused);
    r.tap(ui::menuRow(RowHide));
    CHECK(r.app.visual().paused);
}

TEST(toolbar_swallows_touches_between_its_buttons) {
    Rig r;
    r.openEditor();
    r.tap(kOnA.x, kOnA.y);
    const Rect bar = ui::kToolbar;
    r.tap(bar.x0 + 4, bar.y0 + 4);  // toolbar padding: neither a tool nor empty space
    CHECK(r.scene().selected.kind == ControlRef::ButtonC);
}

TEST(editor_tools_all_fit_inside_the_toolbar_without_overlapping) {
    for (int a = 0; a < kEditTools; a++) {
        const Rect ra = ui::toolButton(a);
        CHECK(ra.x0 >= ui::kToolbar.x0 && ra.x1 <= ui::kToolbar.x1 && ra.y0 >= ui::kToolbar.y0 && ra.y1 <= ui::kToolbar.y1);
        for (int b = a + 1; b < kEditTools; b++) CHECK(!ra.intersects(ui::toolButton(b)));
    }
}

TEST(toolbar_and_menu_stay_clear_of_the_default_controls_and_the_menu_button) {
    const Layout& l = defaultLayout();
    for (int i = 0; i < l.buttonCount; i++) {
        const ButtonDef& b = l.buttons[i];
        const Rect box{b.cx - b.rx, b.cy - b.ry, b.cx + b.rx, b.cy + b.ry};
        CHECK(!box.intersects(ui::kToolbar));
    }
    const Rect stickL{l.stickL.cx - l.stickL.baseRadius, l.stickL.cy - l.stickL.baseRadius, l.stickL.cx + l.stickL.baseRadius, l.stickL.cy + l.stickL.baseRadius};
    const Rect stickR{l.stickR.cx - l.stickR.baseRadius, l.stickR.cy - l.stickR.baseRadius, l.stickR.cx + l.stickR.baseRadius, l.stickR.cy + l.stickR.baseRadius};
    const Rect dpad{l.dpad.cx - l.dpad.armLen, l.dpad.cy - l.dpad.armLen, l.dpad.cx + l.dpad.armLen, l.dpad.cy + l.dpad.armLen};
    CHECK(!stickL.intersects(ui::kToolbar) && !stickR.intersects(ui::kToolbar) && !dpad.intersects(ui::kToolbar));
    const Rect menuButton{l.menu.cx - l.menu.r - l.slop, l.menu.cy - l.menu.r - l.slop, l.menu.cx + l.menu.r + l.slop, l.menu.cy + l.menu.r + l.slop};
    CHECK(!menuButton.intersects(ui::kMenuPanel));
    CHECK(!menuButton.intersects(ui::kToolbar));
}

// ---- game packs: several layouts and moving between them ----

namespace {


// Landmarks of the shipped packs (see packs/*.ini).
constexpr TouchPoint kKartRacePill{0, 756, 44};
constexpr TouchPoint kKartMenuPill{0, 742, 44};
constexpr TouchPoint kKartGas{0, 940, 648};
constexpr TouchPoint kKartPause{0, 1220, 52};
constexpr TouchPoint kRoamEnterOrExit{0, 1201, 379};
constexpr TouchPoint kRoamPhone{0, 60, 52};
constexpr TouchPoint kRoamBackToGame{0, 640, 664};
constexpr TouchPoint kRoamGas{0, 1128, 568};  // the big button: fire on foot, gas in the car

struct GameRig : Rig {
    // The multi-layout pack comes "from the card", then the game starts.
    explicit GameRig(uint64_t game) {
        addRoamPack(app.packs());
        app.setGame(game);
    }
    std::string layoutName() { return scene().layoutName; }
    void tapAt(TouchPoint p) { tap(p.x, p.y); }
    const Pack& pack() { return app.packs().items[app.currentPack()]; }
};

}  // namespace

TEST(a_game_with_a_pack_gets_it_without_being_told) {
    GameRig kart(kMarioKart);
    CHECK(kart.packId() == "mk8d");
    GameRig roam(kRoam);
    CHECK(roam.packId() == "roam");
    CHECK(roam.layoutName() == "On foot");
    CHECK(!roam.app.takeSettingsDirty());  // following the game writes nothing
}

TEST(the_players_own_choice_beats_the_pack_made_for_the_game) {
    Rig r;
    r.app.settings().bind(kMarioKart, "full");
    r.app.setGame(kMarioKart);
    CHECK(r.packId() == "full");
}

TEST(a_game_whose_chosen_pack_is_gone_falls_back_to_the_one_made_for_it) {
    Rig r;
    r.app.settings().bind(kMarioKart, "deleted");
    r.app.setGame(kMarioKart);
    CHECK(r.packId() == "mk8d");
}

TEST(mario_kart_starts_with_the_buttons_out_of_the_way_and_touch_left_to_the_game) {
    GameRig r(kMarioKart);
    CHECK(r.layoutName() == "Menu");
    CHECK(!r.app.blockGameTouch());
    CHECK(r.touch({kKartGas}) == PadState{});  // no gas button here
    r.lift();
    CHECK(r.layoutName() == "Menu");
}

TEST(one_tap_brings_up_the_race_controls_and_takes_touch_back) {
    GameRig r(kMarioKart);
    r.tapAt(kKartRacePill);
    CHECK(r.layoutName() == "Race");
    CHECK(r.app.blockGameTouch());
    CHECK_EQ(r.touch({kKartGas}).buttons, BtnA);
}

TEST(a_touch_through_layout_still_blocks_touch_while_the_overlay_menu_is_open) {
    GameRig r(kMarioKart);
    r.openMenu();
    CHECK(r.app.mode() == Mode::Menu);
    CHECK(r.app.blockGameTouch());
    r.openMenu();
    CHECK(!r.app.blockGameTouch());
}

TEST(pausing_the_race_presses_plus_and_then_gets_out_of_the_way) {
    GameRig r(kMarioKart);
    r.tapAt(kKartRacePill);
    CHECK_EQ(r.touch({kKartPause}).buttons, BtnPlus);
    CHECK(r.layoutName() == "Race");  // not before the game has seen the press
    r.lift();
    CHECK(r.layoutName() == "Menu");
    CHECK(!r.app.blockGameTouch());
}

TEST(latched_gas_is_let_go_when_the_layout_changes) {
    GameRig r(kMarioKart);
    r.tapAt(kKartRacePill);
    r.tapAt(kKartGas);
    CHECK_EQ(r.app.pad().buttons, BtnA);  // latched
    r.tapAt(kKartMenuPill);
    CHECK(r.layoutName() == "Menu");
    CHECK_EQ(r.touch({}).buttons, 0);
    r.tapAt(kKartRacePill);
    CHECK_EQ(r.touch({}).buttons, 0);  // and it does not come back by itself
}

TEST(getting_into_a_car_swaps_the_controls_and_getting_out_swaps_them_back) {
    GameRig r(kRoam);
        CHECK_EQ(r.touch({kRoamEnterOrExit}).buttons, BtnX);
    r.lift();
    CHECK(r.layoutName() == "In vehicle");
    CHECK_EQ(r.touch({kRoamGas}).buttons, BtnZR);
    r.lift();
    CHECK_EQ(r.touch({kRoamEnterOrExit}).buttons, BtnX);
    r.lift();
    CHECK(r.layoutName() == "On foot");
}

TEST(the_thumb_that_was_walking_goes_on_steering_after_the_swap) {
    GameRig r(kRoam);
    r.touch({{1, 250, 520}});
    r.touch({{1, 250, 440}});  // pushing forward
    CHECK(r.app.pad().ly > kStickMax / 2);
    r.touch({{1, 250, 440}, {2, kRoamEnterOrExit.x, kRoamEnterOrExit.y}});
    r.touch({{1, 250, 440}});  // the second finger let go of "enter"
    CHECK(r.layoutName() == "In vehicle");
    CHECK(r.app.pad().ly > kStickMax / 2);
    CHECK_EQ(r.app.pad().buttons, 0);
}

TEST(the_menu_layout_returns_to_wherever_the_player_was) {
    GameRig r(kRoam);
    r.tapAt(kRoamEnterOrExit);  // into the car
    CHECK_EQ(r.touch({kRoamPhone}).buttons, BtnUp);
    r.lift();
    CHECK(r.layoutName() == "Menu");
    CHECK_EQ(r.touch({{0, 200, 640}}).buttons, BtnDown);  // d-pad for the phone
    r.lift();
    r.tapAt(kRoamBackToGame);
    CHECK(r.layoutName() == "In vehicle");
}

TEST(back_with_nowhere_to_go_back_to_lands_on_the_starting_layout) {
    GameRig r(kRoam);
    r.openMenu();
    r.tap(ui::menuArrow(RowLayout, false));  // picked by hand: no history
    r.openMenu();
    CHECK(r.layoutName() == "Menu");
    r.tapAt(kRoamBackToGame);
    CHECK(r.layoutName() == "On foot");
}

TEST(layout_row_steps_through_the_layouts_of_the_pack) {
    GameRig r(kRoam);
    r.openMenu();
    CHECK(std::string(r.scene().rowValue[RowPack]) == "Open World");
    CHECK(std::string(r.scene().rowValue[RowLayout]) == "On foot");
    r.tap(ui::menuArrow(RowLayout, true));
    CHECK(std::string(r.scene().rowValue[RowLayout]) == "In vehicle");
    CHECK_EQ(r.app.currentLayout(), 1);
    for (int i = 1; i < r.pack().layoutCount; i++) r.tap(ui::menuArrow(RowLayout, true));
    CHECK_EQ(r.app.currentLayout(), 0);  // wraps
    CHECK(!r.app.takeSettingsDirty());   // nothing to remember: the game decides the layout
}

TEST(layout_row_on_a_one_layout_pack_changes_nothing) {
    Rig r;
    r.openMenu();
    r.tap(ui::menuArrow(RowLayout, true));
    CHECK_EQ(r.app.currentLayout(), 0);
    CHECK(r.packId() == "full");
}

TEST(choosing_another_pack_starts_on_its_first_layout) {
    GameRig r(kRoam);
    r.tapAt(kRoamEnterOrExit);
    r.openMenu();
    r.tap(ui::menuArrow(RowPack, false));  // the pack before it in the list
    CHECK(r.packId() == "burnout");
    CHECK(r.layoutName() == "Drive");
    r.tap(ui::menuArrow(RowPack, true));
    CHECK(r.layoutName() == "On foot");
}

TEST(no_layout_switch_happens_while_the_overlay_menu_is_open) {
    GameRig r(kRoam);
    r.openMenu();
    r.tapAt(kRoamEnterOrExit);  // outside the panel: closes the menu, presses nothing
    CHECK(r.layoutName() == "On foot");
}

TEST(editing_changes_only_the_layout_on_screen) {
    GameRig r(kRoam);
    const int roam = r.app.currentPack();
    r.tapAt(kRoamEnterOrExit);
    r.openEditor();
    r.drag(kRoamGas.x, kRoamGas.y, 900, 250);
    r.tap(ui::toolButton(ToolSave));
    CHECK_EQ(r.app.takePackToSave(), roam);
    CHECK(r.layoutName() == "In vehicle");  // saving keeps you where you were
    const Pack& p = r.app.packs().items[roam];
    CHECK_EQ(p.layouts[1].layout.buttons[0].cx, 900);
    CHECK_EQ(p.layouts[1].layout.buttons[0].cy, 250);
    CHECK_EQ(p.layouts[0].layout.buttons[0].cx, 1128);  // "on foot" untouched
    CHECK_EQ(r.touch({{0, 900, 250}}).buttons, BtnZR);
}

TEST(save_as_new_copies_the_whole_pack_and_binds_it_to_the_game) {
    GameRig r(kRoam);
    const int before = r.app.packs().count;
    const int roam = r.app.currentPack();
    r.openEditor();
    r.tap(ui::toolButton(ToolSaveNew));
    CHECK_EQ(r.app.packs().count, before + 1);
    CHECK(r.packId() == "custom1");
    CHECK_EQ(r.pack().layoutCount, r.app.packs().items[roam].layoutCount);
    CHECK_EQ(r.pack().gameCount, 0);  // the original stays the game's own pack
    CHECK(std::string(r.app.settings().packFor(kRoam)) == "custom1");
    r.tapAt(kRoamEnterOrExit);
    CHECK(r.layoutName() == "In vehicle");  // the copy switches layouts just the same
}

TEST(scene_names_the_pack_and_the_layout) {
    GameRig r(kRoam);
    CHECK(std::string(r.scene().packName) == "Open World");
    CHECK(std::string(r.scene().layoutName) == "On foot");
    Rig plain;
    CHECK(std::string(plain.scene().packName) == "All buttons");
}

// ---- the full gamepad as a way out of a game pack ----

TEST(full_pad_row_puts_the_whole_gamepad_over_a_game_pack) {
    GameRig r(kRoam);
    r.openMenu();
    CHECK(std::string(r.scene().rowValue[RowFullPad]) == "off");
    r.tap(ui::menuRow(RowFullPad));
    CHECK(r.app.fullPad());
    CHECK(std::string(r.scene().rowValue[RowFullPad]) == "on");
    CHECK(r.app.mode() == Mode::Menu);
    r.openMenu();
    CHECK_EQ(r.touch({kOnA}).buttons, BtnA);  // the plain pad's A, where the pack has nothing
    r.lift();
    CHECK_EQ(r.scene().layout.buttonCount, 14);
    CHECK(r.layoutName() == "Full gamepad");
    CHECK(r.packId() == "roam");           // the game keeps its pack
    CHECK(!r.app.takeSettingsDirty());     // and nothing is remembered about it
}

TEST(full_pad_row_again_brings_back_the_layout_that_was_up) {
    GameRig r(kRoam);
    r.tapAt(kRoamEnterOrExit);  // in the car
    r.openMenu();
    r.tap(ui::menuRow(RowFullPad));
    r.tap(ui::menuRow(RowFullPad));
    r.openMenu();
    CHECK(!r.app.fullPad());
    CHECK(r.layoutName() == "In vehicle");
    CHECK_EQ(r.touch({kRoamGas}).buttons, BtnZR);
}

TEST(full_pad_never_switches_layouts_by_itself) {
    GameRig r(kRoam);
    r.openMenu();
    r.tap(ui::menuRow(RowFullPad));
    r.openMenu();
    r.tapAt(kRoamEnterOrExit);  // where "enter the car" is in the pack
    CHECK(r.app.fullPad());
    CHECK_EQ(r.app.currentLayout(), 0);
}

TEST(full_pad_takes_touch_away_from_the_game_even_over_a_touch_through_layout) {
    GameRig r(kMarioKart);
    CHECK(!r.app.blockGameTouch());
    r.openMenu();
    r.tap(ui::menuRow(RowFullPad));
    r.openMenu();
    CHECK(r.app.blockGameTouch());
    CHECK_EQ(r.touch({kOnA}).buttons, BtnA);
}

TEST(choosing_a_pack_or_a_layout_by_hand_turns_the_full_pad_off) {
    GameRig r(kRoam);
    r.openMenu();
    r.tap(ui::menuRow(RowFullPad));
    r.tap(ui::menuArrow(RowLayout, true));
    CHECK(!r.app.fullPad());
    CHECK(r.layoutName() == "In vehicle");
    r.tap(ui::menuRow(RowFullPad));
    r.tap(ui::menuArrow(RowPack, false));
    CHECK(!r.app.fullPad());
    CHECK(r.packId() == "burnout");
}

TEST(a_new_game_starts_with_its_pack_not_the_full_pad) {
    GameRig r(kRoam);
    r.openMenu();
    r.tap(ui::menuRow(RowFullPad));
    r.openMenu();
    r.app.setGame(kMarioKart);
    CHECK(!r.app.fullPad());
    CHECK(r.layoutName() == "Menu");
}

TEST(the_editor_opens_on_the_packs_layout_not_on_the_stand_in) {
    GameRig r(kRoam);
    const int roam = r.app.currentPack();
    r.openMenu();
    r.tap(ui::menuRow(RowFullPad));
    r.tap(ui::menuRow(RowEdit));
    CHECK(r.app.mode() == Mode::Edit);
    CHECK(!r.app.fullPad());
    r.tap(ui::toolButton(ToolSave));
    // What was saved is still the pack's layout: the fire button is where the pack put it.
    CHECK_EQ(r.app.packs().items[roam].layouts[0].layout.buttons[0].cx, 1128);
    CHECK_EQ(r.app.packs().items[roam].layouts[0].layout.buttonCount, 13);
    CHECK(r.app.packs().items[roam].layouts[0].layout.buttons[0].bit == BtnZR);
}

// ---- coming from 0.2 ----

TEST(a_profile_remembered_by_0_2_gives_way_to_the_games_own_pack_once) {
    Rig r;
    addRoamPack(r.app.packs());
    // The card of someone who played two games under 0.2 that have packs of their own now.
    parseSettings("[general]\nprofile=dpad\n[games]\n0107306AC97A0000=full\n0100152000022000=racing\n0100AAAA00002000=full\n",
                  r.app.settings());
    r.app.adoptLegacySettings();
    CHECK(r.app.takeSettingsDirty());  // to be rewritten as a 0.3 file
    CHECK(r.app.settings().packFor(kRoam) == nullptr);
    CHECK(r.app.settings().packFor(kMarioKart) == nullptr);
    CHECK(std::string(r.app.settings().packFor(0x0107306AC97A0000ull)) == "full");  // no pack for it: choice kept
    r.app.setGame(kRoam);
    CHECK(r.packId() == "roam");
    r.app.setGame(0);
    CHECK(r.packId() == "dpad");
}

TEST(a_choice_made_under_0_3_is_not_second_guessed) {
    Rig r;
    addRoamPack(r.app.packs());
    parseSettings("[general]\npack=full\n[games]\n0100AAAA00002000=full\n", r.app.settings());
    r.app.adoptLegacySettings();
    CHECK(!r.app.takeSettingsDirty());
    r.app.setGame(kRoam);
    CHECK(r.packId() == "full");
}

TEST(full_pad_row_shows_the_players_own_arrangement_of_the_full_pad) {
    GameRig r(kRoam);
    r.app.packs().items[r.app.packs().find("full")].layouts[0].layout.buttons[4].cx = 900;  // A moved by the player
    r.openMenu();
    r.tap(ui::menuRow(RowFullPad));
    r.openMenu();
    CHECK_EQ(r.touch({{0, 900, 300}}).buttons, BtnA);
}

TEST(full_pad_row_works_even_without_a_full_pack) {
    GameRig r(kRoam);
    PackStore& s = r.app.packs();
    std::strcpy(s.items[s.find("full")].id, "renamed");
    r.openMenu();
    r.tap(ui::menuRow(RowFullPad));
    r.openMenu();
    CHECK_EQ(r.touch({kOnA}).buttons, BtnA);
}

// ---- layouts that follow what the game reports ----

namespace {

constexpr uint32_t kAlive = 0x40000000 | 0x100000;  // the character exists and the player has control
constexpr uint32_t kInVehicle = 0x1, kPlane = 0x2, kEntering = 0x40, kPhone = 0x400, kPause = 0x10000;

}  // namespace

TEST(without_a_report_from_the_game_nothing_changes_by_itself) {
    GameRig r(kRoam);
    CHECK(r.app.wantsGameState());
    CHECK(!r.app.gameStateKnown());
    r.app.setGameState(false, kInVehicle);
    CHECK(r.layoutName() == "On foot");
    r.tapAt(kRoamEnterOrExit);  // buttons still do the switching
    CHECK(r.layoutName() == "In vehicle");
    Rig plain;
    CHECK(!plain.app.wantsGameState());
}

TEST(getting_into_a_car_in_the_game_brings_up_the_driving_layout) {
    GameRig r(kRoam);
    r.app.setGameState(true, kAlive);
    CHECK(r.layoutName() == "On foot");
    r.app.setGameState(true, kAlive | kEntering);
    CHECK(r.layoutName() == "In vehicle");
    r.app.setGameState(true, kAlive | kInVehicle);
    CHECK(r.layoutName() == "In vehicle");
    CHECK_EQ(r.touch({kRoamGas}).buttons, BtnZR);
    r.lift();
    r.app.setGameState(true, kAlive);
    CHECK(r.layoutName() == "On foot");
    CHECK(!r.app.takeSettingsDirty());
}

TEST(a_plane_gets_its_own_layout) {
    GameRig r(kRoam);
    r.app.setGameState(true, kAlive | kInVehicle | kPlane);
    CHECK(r.layoutName() == "Aircraft");
    r.app.setGameState(true, kAlive | kInVehicle);
    CHECK(r.layoutName() == "In vehicle");
}

TEST(pausing_the_game_brings_the_menu_layout_and_resuming_takes_it_away) {
    GameRig r(kRoam);
    r.app.setGameState(true, kAlive | kInVehicle);
    r.app.setGameState(true, kAlive | kInVehicle | kPause);
    CHECK(r.layoutName() == "Menu");
    CHECK_EQ(r.touch({{0, 200, 640}}).buttons, BtnDown);
    r.lift();
    r.app.setGameState(true, kAlive | kInVehicle);
    CHECK(r.layoutName() == "In vehicle");
}

TEST(a_phone_call_leaves_the_layout_alone_and_the_phone_button_opens_the_menu_to_stay) {
    GameRig r(kRoam);
    r.app.setGameState(true, kAlive | kInVehicle);
    r.app.setGameState(true, kAlive | kInVehicle | kPhone);  // it rings
    CHECK(r.layoutName() == "In vehicle");
    CHECK_EQ(r.touch({kRoamPhone}).buttons, BtnUp);  // answer
    r.lift();
    CHECK(r.layoutName() == "Menu");                // asked for by the player: d-pad for the phone
    r.app.setGameState(true, kAlive | kInVehicle);  // the game reports on; the menu holds
    r.app.setGameState(true, kAlive | kInVehicle | kPause);
    r.app.setGameState(true, kAlive);               // got out of the car meanwhile
    CHECK(r.layoutName() == "Menu");
    r.tapAt(kRoamBackToGame);
    CHECK(r.layoutName() == "On foot");             // back to where the game is now
    r.app.setGameState(true, kAlive | kInVehicle);
    CHECK(r.layoutName() == "In vehicle");          // and following again
}

TEST(with_a_report_buttons_no_longer_decide_between_the_games_own_layouts) {
    GameRig r(kRoam);
    r.app.setGameState(true, kAlive);
    CHECK_EQ(r.touch({kRoamEnterOrExit}).buttons, BtnX);  // the press still goes to the game
    r.lift();
    CHECK(r.layoutName() == "On foot");                  // no car nearby: the game says still on foot
}

TEST(the_steering_thumb_survives_a_switch_made_by_the_game) {
    GameRig r(kRoam);
    r.app.setGameState(true, kAlive);
    r.touch({{1, 250, 520}});
    r.touch({{1, 250, 440}});
    r.app.setGameState(true, kAlive | kInVehicle);
    CHECK(r.layoutName() == "In vehicle");
    CHECK(r.touch({{1, 250, 440}}).ly > kStickMax / 2);
}

TEST(losing_the_report_hands_the_switching_back_to_the_buttons) {
    GameRig r(kRoam);
    r.app.setGameState(true, kAlive | kInVehicle);
    r.app.setGameState(false, 0);
    CHECK(r.layoutName() == "In vehicle");  // stays where it was
    r.tapAt(kRoamEnterOrExit);
    CHECK(r.layoutName() == "On foot");
}

TEST(the_full_pad_is_not_swapped_out_by_the_game) {
    GameRig r(kRoam);
    r.app.setGameState(true, kAlive);
    r.openMenu();
    r.tap(ui::menuRow(RowFullPad));
    r.openMenu();
    r.app.setGameState(true, kAlive | kInVehicle);
    CHECK(r.app.fullPad());
    CHECK_EQ(r.touch({kOnA}).buttons, BtnA);
    r.lift();
    r.openMenu();
    r.tap(ui::menuRow(RowFullPad));  // back to the pack: straight to what the game is doing now
    r.openMenu();
    CHECK(r.layoutName() == "In vehicle");
}

TEST(the_editor_is_not_swapped_out_by_the_game_and_catches_up_afterwards) {
    GameRig r(kRoam);
    r.app.setGameState(true, kAlive);
    r.openEditor();
    r.app.setGameState(true, kAlive | kInVehicle);
    CHECK(r.app.mode() == Mode::Edit);
    CHECK_EQ(r.app.currentLayout(), r.pack().findLayout("foot"));
    r.tap(ui::toolButton(ToolCancel));
    CHECK(r.layoutName() == "In vehicle");
}

TEST(a_new_game_with_a_report_already_known_starts_in_the_right_layout) {
    GameRig r(kOtherGame);
    r.app.setGameState(true, kAlive | kInVehicle);
    CHECK(r.packId() == "full");  // no rules there: nothing happens
    r.app.setGame(kRoam);
    CHECK(r.layoutName() == "In vehicle");
}

TEST(a_layout_opened_by_hand_stays_and_back_lands_where_the_game_is_now) {
    Rig r;
    static Pack p;
    std::strcpy(p.id, "byhand");
    CHECK(parsePack("[state]\ncar=1\n[foot]\nwhen=!car\nbtn=A | 1000,300,50\nbtn=NONE | 300,300,50 | goto=extra\n"
                    "[drive]\nwhen=car\nbtn=B | 1000,300,50\n[extra]\nbtn=X | 1000,300,50\nbtn=NONE | 300,300,50 | goto=back\n",
                    p));
    r.app.usePack(r.app.packs().put(p));
    r.app.setGameState(true, 0);
    r.tap(300, 300);  // open the manual layout from "foot"
    CHECK_EQ(r.app.currentLayout(), 2);
    r.app.setGameState(true, 1);  // the game moved on meanwhile
    CHECK_EQ(r.app.currentLayout(), 2);
    r.tap(300, 300);  // back
    CHECK_EQ(r.app.currentLayout(), 1);
}
