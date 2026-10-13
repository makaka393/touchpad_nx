#include "tinytest.hpp"
#include "../core/config.hpp"
#include "roam_pack.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

using namespace tp;

namespace {

const ButtonDef* button(const Layout& l, Button bit) {
    for (int i = 0; i < l.buttonCount; i++)
        if (l.buttons[i].bit == bit) return &l.buttons[i];
    return nullptr;
}

// A pack held outside the stack: it is several kilobytes.
Pack& scratch(const char* id = "test") {
    static Pack p;
    std::strcpy(p.id, id);
    return p;
}

// One-layout stand-in for a real pack.
Pack& fullPack(const char* id) {
    static Pack p;
    std::strcpy(p.id, id);
    std::strcpy(p.name, "All buttons");
    p.author[0] = 0;
    p.gameCount = 0;
    p.layoutCount = 1;
    p.start = 0;
    std::strcpy(p.layouts[0].id, "main");
    std::strcpy(p.layouts[0].name, "All buttons");
    p.layouts[0].layout = defaultLayout();
    return p;
}

PackStore& emptyStore() {
    static PackStore s;
    s.count = 0;
    return s;
}

constexpr uint64_t kMarioKart = 0x0100152000022000ull;

const char kSample[] =
    "; sample\n"
    "[pack]\n"
    "name=Проба\n"
    "author=кто-то\n"
    "games=0100152000022000, 0100AAAA00002000\n"
    "start=drive\n"
    "\n"
    "[walk]\n"
    "name=On foot\n"
    "menu=600,50\n"
    "stick_l=200,500,100 | zone=0,200,560,720\n"
    "stick_r=900,330,90 | zone=580,100,1280,720 | hint=0\n"
    "btn=ZR | 1100,600,60 | icon=fire | color=#F54 | drag=rs\n"
    "btn=A | 1000,640,50 | icon=run | mode=turbo\n"
    "btn=ZL | 1200,450,48 | mode=toggle\n"
    "btn=Y | 1200,330,46 | icon=car | label=В АВТО | goto=drive\n"
    "btn=NONE | 760,44,46,28 | label=МЕНЮ | goto=back\n"
    "\n"
    "[drive]\n"
    "name=In vehicle\n"
    "touch=game\n"
    "dpad=300,500,110\n"
    "btn=ZL+ZR | 640,360,90,40\n"
    "btn=Y | 1200,330,46 | goto=walk | hidden=1\n";

}  // namespace

// ---- pack store and built-ins ----

TEST(store_finds_packs_by_id) {
    PackStore& s = emptyStore();
    CHECK_EQ(s.put(fullPack("full")), 0);
    CHECK_EQ(s.put(fullPack("other")), 1);
    CHECK_EQ(s.find("other"), 1);
    CHECK_EQ(s.find("nope"), -1);
    CHECK_EQ(s.count, 2);
}

TEST(store_replaces_a_pack_with_the_same_id) {
    PackStore& s = emptyStore();
    Pack& a = fullPack("full");
    s.put(a);
    std::strcpy(a.name, "Изменённый");
    a.layouts[0].layout.stickR.visible = false;
    CHECK_EQ(s.put(a), 0);
    CHECK_EQ(s.count, 1);
    CHECK(std::string(s.items[0].name) == "Изменённый");
    CHECK(!s.items[0].layouts[0].layout.stickR.visible);
}

TEST(store_refuses_more_packs_than_it_can_hold) {
    PackStore& s = emptyStore();
    for (int i = 0; i < kMaxPacks; i++) {
        char id[16];
        std::snprintf(id, sizeof(id), "p%d", i);
        CHECK_EQ(s.put(fullPack(id)), i);
    }
    CHECK_EQ(s.put(fullPack("extra")), -1);
    CHECK_EQ(s.count, kMaxPacks);
}

TEST(builtin_full_pack_shows_everything) {
    PackStore& s = emptyStore();
    addBuiltinPacks(s);
    const int i = s.find("full");
    CHECK(i == 0);  // it is the default, so it comes first
    if (i < 0) return;
    CHECK_EQ(s.items[i].layoutCount, 1);
    const Layout& l = s.items[i].layouts[0].layout;
    CHECK_EQ(l.buttonCount, 14);
    for (int b = 0; b < l.buttonCount; b++) CHECK(l.buttons[b].visible);
    CHECK(l.stickL.visible && l.stickR.visible && l.dpad.visible);
}

TEST(there_is_no_catch_all_racing_pack_any_more) {
    PackStore& s = emptyStore();
    addBuiltinPacks(s);
    CHECK_EQ(s.find("racing"), -1);
    CHECK(s.find("mk8d") >= 0);
}

TEST(builtin_packs_have_unique_ids_names_and_something_to_press) {
    PackStore& s = emptyStore();
    addBuiltinPacks(s);
    CHECK(s.count >= 6);
    for (int i = 0; i < s.count; i++) {
        const Pack& p = s.items[i];
        CHECK(p.id[0] != 0 && p.name[0] != 0);
        for (int j = i + 1; j < s.count; j++) CHECK(std::strcmp(p.id, s.items[j].id) != 0);
        CHECK(p.layoutCount >= 1);
        CHECK(p.start >= 0 && p.start < p.layoutCount);
        for (int at = 0; at < p.layoutCount; at++) {
            const Layout& l = p.layouts[at].layout;
            int visible = 0;
            for (int b = 0; b < l.buttonCount; b++) visible += l.buttons[b].visible;
            CHECK(visible >= 1);
            CHECK(p.layouts[at].name[0] != 0);
        }
    }
}

TEST(generic_packs_can_always_reach_plus) {
    PackStore& s = emptyStore();
    addBuiltinPacks(s);
    for (const char* id : {"full", "platformer", "dpad", "minimal"}) {
        const int i = s.find(id);
        CHECK(i >= 0);
        if (i < 0) continue;
        // Without Plus most games cannot be paused or started.
        CHECK(button(s.items[i].layouts[0].layout, BtnPlus)->visible);
    }
}

TEST(shipped_game_packs_read_without_a_single_complaint) {
    int n = 0;
    const char* id = nullptr;
    while (const char* text = builtinPackText(n, &id)) {
        Pack& p = scratch(id);
        PackReport report;
        CHECK(parsePack(text, p, &report));
        if (report.badLines) std::printf("  %s line %d: %s\n", id, report.firstBadLine, report.firstBad);
        CHECK_EQ(report.badLines, 0);
        n++;
    }
    CHECK(n >= 2);
}

// Every layout of a shipped pack can be entered (by a button, or by the game's state) and left
// by touch alone; otherwise the player is stuck in it, or never sees it, until they open the
// overlay's own menu.
TEST(shipped_game_pack_layouts_are_all_reachable_and_none_is_a_dead_end) {
    PackStore& s = emptyStore();
    addBuiltinPacks(s);
    for (int i = 0; i < s.count; i++) {
        const Pack& p = s.items[i];
        if (p.layoutCount < 2) continue;
        bool reached[kMaxLayouts] = {};
        reached[p.start] = true;
        for (int round = 0; round < p.layoutCount; round++) {
            for (int at = 0; at < p.layoutCount; at++) {
                if (!reached[at]) continue;
                const Layout& l = p.layouts[at].layout;
                for (int b = 0; b < l.buttonCount; b++) {
                    if (l.buttons[b].visible && l.buttons[b].gotoLayout >= 0) reached[l.buttons[b].gotoLayout] = true;
                }
            }
        }
        for (int at = 0; at < p.layoutCount; at++) {
            CHECK(reached[at] || p.layouts[at].when.terms > 0);
            const Layout& l = p.layouts[at].layout;
            bool leaves = false;
            for (int b = 0; b < l.buttonCount; b++) leaves |= l.buttons[b].visible && l.buttons[b].gotoLayout != kGotoNone;
            CHECK(leaves);
        }
    }
}

TEST(game_packs_claim_their_games) {
    PackStore& s = emptyStore();
    addBuiltinPacks(s);
    CHECK_EQ(s.findForGame(0x0100DCA0064A6000ull), s.find("lm3"));
    CHECK_EQ(s.findForGame(kMarioKart), s.find("mk8d"));
    CHECK_EQ(s.findForGame(0x0100AAAA00001000ull), -1);
    CHECK_EQ(s.findForGame(0), -1);  // no game running
}

TEST(a_pack_from_the_card_beats_the_builtin_one_for_the_same_game) {
    PackStore& s = emptyStore();
    addBuiltinPacks(s);
    Pack& mine = fullPack("mykart");
    mine.games[0] = kMarioKart;
    mine.gameCount = 1;
    const int at = s.put(mine);
    CHECK_EQ(s.findForGame(kMarioKart), at);
}

TEST(stick_size_drives_its_touch_zones) {
    StickDef s = defaultLayout().stickL;
    setStickSize(s, 105);
    CHECK_EQ(s.baseRadius, 105);
    CHECK_EQ(s.zoneRadius, 140);
    CHECK_EQ(s.throwRadius, 85);
    setStickSize(s, 60);
    CHECK_EQ(s.zoneRadius, 95);
    CHECK(s.throwRadius >= 45 && s.throwRadius <= 50);
}

TEST(dpad_size_drives_its_arms_and_zone) {
    DpadDef d = defaultLayout().dpad;
    setDpadSize(d, 105);
    CHECK_EQ(d.armLen, 105);
    CHECK_EQ(d.armHalfWidth, 38);
    CHECK_EQ(d.zoneRadius, 125);
}

// ---- pack text ----

TEST(pack_text_describes_the_pack_itself) {
    Pack& p = scratch();
    PackReport report;
    CHECK(parsePack(kSample, p, &report));
    CHECK_EQ(report.badLines, 0);
    CHECK(std::string(p.id) == "test");  // the id is the file name, the text cannot change it
    CHECK(std::string(p.name) == "Проба");
    CHECK(std::string(p.author) == "кто-то");
    CHECK_EQ(p.gameCount, 2);
    CHECK(p.madeFor(kMarioKart) && p.madeFor(kRoam) && !p.madeFor(1));
    CHECK_EQ(p.layoutCount, 2);
    CHECK(std::string(p.layouts[0].id) == "walk" && std::string(p.layouts[0].name) == "On foot");
    CHECK(std::string(p.layouts[1].id) == "drive" && std::string(p.layouts[1].name) == "In vehicle");
    CHECK_EQ(p.start, 1);
}

TEST(pack_text_places_buttons_and_gives_them_their_behaviour) {
    Pack& p = scratch();
    parsePack(kSample, p);
    const Layout& l = p.layouts[0].layout;
    CHECK_EQ(l.buttonCount, 5);

    const ButtonDef& fire = l.buttons[0];
    CHECK_EQ(fire.bit, BtnZR);
    CHECK(fire.shape == Shape::Circle);
    CHECK_EQ(fire.cx, 1100);
    CHECK_EQ(fire.cy, 600);
    CHECK_EQ(fire.rx, 60);
    CHECK_EQ(fire.ry, 60);
    CHECK(fire.glyph == Glyph::Fire);
    CHECK(fire.tinted && fire.r == 15 && fire.g == 5 && fire.b == 4);
    CHECK(fire.drag == Drag::StickR);
    CHECK(fire.press == Press::Hold);
    CHECK_EQ(fire.gotoLayout, kGotoNone);

    CHECK(l.buttons[1].press == Press::Turbo);
    CHECK(l.buttons[2].press == Press::Toggle);
    CHECK(!l.buttons[2].tinted);

    const ButtonDef& enter = l.buttons[3];
    CHECK(enter.glyph == Glyph::Car);
    CHECK(std::string(enter.label) == "В АВТО");
    CHECK_EQ(enter.gotoLayout, 1);  // [drive] is defined further down the file

    const ButtonDef& pill = l.buttons[4];
    CHECK_EQ(pill.bit, 0);
    CHECK(pill.shape == Shape::RoundRect);
    CHECK_EQ(pill.rx, 46);
    CHECK_EQ(pill.ry, 28);
    CHECK_EQ(pill.gotoLayout, kGotoBack);
}

TEST(pack_text_names_a_button_after_its_key_when_nothing_else_is_given) {
    Pack& p = scratch();
    parsePack(kSample, p);
    CHECK(p.layouts[0].layout.buttons[2].glyph == Glyph::ZL);
    const ButtonDef& both = p.layouts[1].layout.buttons[0];
    CHECK_EQ(both.bit, BtnZL | BtnZR);
    CHECK(both.glyph == Glyph::None);
    CHECK(std::string(both.label) == "ZL+ZR");
}

TEST(pack_text_leaves_out_what_a_layout_does_not_mention) {
    Pack& p = scratch();
    parsePack(kSample, p);
    const Layout& walk = p.layouts[0].layout;
    CHECK(walk.stickL.visible && walk.stickR.visible);
    CHECK(!walk.dpad.visible);
    CHECK(!walk.touchToGame);
    CHECK_EQ(walk.menu.cx, 600);
    CHECK_EQ(walk.menu.cy, 50);

    const Layout& drive = p.layouts[1].layout;
    CHECK(!drive.stickL.visible && !drive.stickR.visible);
    CHECK(drive.dpad.visible);
    CHECK_EQ(drive.dpad.armLen, 110);
    CHECK(drive.touchToGame);
    CHECK_EQ(drive.menu.cx, 640);  // default place
    CHECK(!drive.buttons[1].visible);  // hidden=1 keeps the control for the editor
}

TEST(pack_text_makes_sticks_float_inside_a_zone) {
    Pack& p = scratch();
    parsePack(kSample, p);
    const StickDef& l = p.layouts[0].layout.stickL;
    CHECK(l.floating && l.hint);
    CHECK_EQ(l.cx, 200);
    CHECK_EQ(l.baseRadius, 100);
    CHECK_EQ(l.zx0, 0);
    CHECK_EQ(l.zy0, 200);
    CHECK_EQ(l.zx1, 560);
    CHECK_EQ(l.zy1, 720);
    const StickDef& r = p.layouts[0].layout.stickR;
    CHECK(r.floating && !r.hint);
}

TEST(pack_text_survives_hand_editing) {
    Pack& p = scratch();
    PackReport report;
    // Windows line endings, stray spaces, comments in both styles, blank lines.
    CHECK(parsePack("# mine\r\n[pack]\r\n name = Моё \r\n\r\n[ main ]\r\n; pad\r\n  btn =  a |  300 , 310 , 50  |  label = Прыг \r\n"
                    "stick_l = 700,500,90\r\n",
                    p, &report));
    CHECK_EQ(report.badLines, 0);
    CHECK(std::string(p.name) == "Моё");
    CHECK_EQ(p.layoutCount, 1);
    const Layout& l = p.layouts[0].layout;
    CHECK_EQ(l.buttonCount, 1);
    CHECK_EQ(l.buttons[0].bit, BtnA);
    CHECK_EQ(l.buttons[0].cx, 300);
    CHECK_EQ(l.buttons[0].cy, 310);
    CHECK(std::string(l.buttons[0].label) == "Прыг");
    CHECK_EQ(l.stickL.cx, 700);
    CHECK_EQ(l.stickL.baseRadius, 90);
    CHECK(!l.stickL.floating);
}

TEST(pack_text_with_mistakes_keeps_the_good_lines_and_points_at_the_first_bad_one) {
    Pack& p = scratch();
    PackReport report;
    CHECK(parsePack("[pack]\nname=X\n[main]\nbtn=A | 100,100,40\nbtn=Q | 200,200,40\nbtn=B | 300,300\nwhat=1\n"
                    "btn=X | 400,400,40 | icon=unicorn\nbtn=Y | 500,500,40 | goto=nowhere\nbtn=L | 600,600,40 | mode=sticky\n",
                    p, &report));
    CHECK_EQ(p.layouts[0].layout.buttonCount, 1);
    CHECK_EQ(report.badLines, 6);
    CHECK_EQ(report.firstBadLine, 5);
    CHECK(std::string(report.firstBad) == "btn=Q | 200,200,40");
}

TEST(pack_text_without_a_layout_is_no_pack) {
    Pack& p = scratch();
    CHECK(!parsePack("[pack]\nname=Пусто\n", p));
    CHECK(!parsePack("", p));
}

TEST(pack_text_cannot_push_controls_off_screen_or_to_absurd_sizes) {
    Pack& p = scratch();
    parsePack("[main]\nbtn=A | 5000,-40,900\nbtn=B | -9,9999,900,900\nstick_l=-500,9999,3\ndpad=99999,-5,2\nmenu=-50,5000\n", p);
    const Layout& l = p.layouts[0].layout;
    CHECK_EQ(l.buttonCount, 2);
    for (int i = 0; i < l.buttonCount; i++) {
        const ButtonDef& b = l.buttons[i];
        CHECK(b.rx >= 20 && b.rx <= 220 && b.ry >= 20 && b.ry <= 120);
        CHECK(b.cx - b.rx >= 0 && b.cx + b.rx <= kScreenW && b.cy - b.ry >= 0 && b.cy + b.ry <= kScreenH);
    }
    CHECK(l.stickL.baseRadius >= 60);
    CHECK(l.stickL.cx - l.stickL.baseRadius >= 0);
    CHECK(l.stickL.cy + l.stickL.baseRadius <= kScreenH);
    CHECK(l.dpad.cx + l.dpad.armLen <= kScreenW && l.dpad.cy - l.dpad.armLen >= 0);
    CHECK(l.menu.cx - l.menu.r >= 0 && l.menu.cy + l.menu.r <= kScreenH);
}

TEST(pack_text_holds_no_more_than_a_layout_can) {
    std::string text = "[main]\n";
    for (int i = 0; i < Layout::kMaxButtons + 3; i++) text += "btn=A | 100,100,40\n";
    Pack& p = scratch();
    PackReport report;
    CHECK(parsePack(text.c_str(), p, &report));
    CHECK_EQ(p.layouts[0].layout.buttonCount, Layout::kMaxButtons);
    CHECK_EQ(report.badLines, 3);

    std::string many;
    for (int i = 0; i < kMaxLayouts + 2; i++) many += "[l" + std::to_string(i) + "]\nbtn=A | 100,100,40\n";
    PackReport second;
    CHECK(parsePack(many.c_str(), p, &second));
    CHECK_EQ(p.layoutCount, kMaxLayouts);
    CHECK_EQ(second.badLines, 2);
}

TEST(pack_text_round_trips_everything_the_editor_can_change) {
    Pack& p = scratch();
    parsePack(kSample, p);
    Layout& l = p.layouts[0].layout;
    l.buttons[0].cx = 1000;
    l.buttons[0].rx = l.buttons[0].ry = 64;
    l.buttons[1].visible = false;
    l.stickR.visible = false;
    l.stickL.cx = 220;
    setStickSize(l.stickL, 120);

    static char text[16 * 1024];
    const int len = writePack(p, text, sizeof(text));
    CHECK(len > 200 && len < static_cast<int>(sizeof(text)));

    static Pack q;
    std::strcpy(q.id, "test");
    PackReport report;
    CHECK(parsePack(text, q, &report));
    CHECK_EQ(report.badLines, 0);
    const Layout& m = q.layouts[0].layout;
    CHECK_EQ(m.buttons[0].cx, 1000);
    CHECK_EQ(m.buttons[0].rx, 64);
    CHECK(!m.buttons[1].visible);
    CHECK(m.buttons[2].visible);
    CHECK(!m.stickR.visible);
    CHECK(m.stickR.floating && !m.stickR.hint);  // still knows how to come back
    CHECK_EQ(m.stickL.cx, 220);
    CHECK_EQ(m.stickL.baseRadius, 120);
    CHECK(m.stickL.floating);
    CHECK_EQ(q.start, 1);
    CHECK(std::string(q.layouts[1].name) == "In vehicle");
    CHECK(q.layouts[1].layout.touchToGame);
    CHECK_EQ(m.buttons[3].gotoLayout, 1);
    CHECK_EQ(m.buttons[4].gotoLayout, kGotoBack);
}

TEST(pack_writer_reports_truncation_instead_of_overflowing) {
    Pack& p = scratch();
    parsePack(kSample, p);
    char tiny[64];
    CHECK_EQ(writePack(p, tiny, sizeof(tiny)), 0);
}

// ---- settings text ----

TEST(settings_round_trip) {
    Settings s;
    s.theme.r = 15;
    s.theme.g = 9;
    s.theme.b = 3;
    s.theme.opacity = 2;
    s.theme.faceColors = true;
    std::strcpy(s.defaultPack, "platformer");
    s.bind(0x0100152000022000ull, "mk8d");
    s.bind(0x01007EF00011E000ull, "platformer");

    char text[4096];
    const int len = writeSettings(s, text, sizeof(text));
    CHECK(len > 40);

    Settings q;
    parseSettings(text, q);
    CHECK(q.theme == s.theme);
    CHECK(std::string(q.defaultPack) == "platformer");
    CHECK_EQ(q.gameCount, 2);
    CHECK(q.packFor(0x0100152000022000ull) && std::string(q.packFor(0x0100152000022000ull)) == "mk8d");
    CHECK(q.packFor(0x01007EF00011E000ull) && std::string(q.packFor(0x01007EF00011E000ull)) == "platformer");
}

TEST(settings_unknown_game_has_no_pack) {
    Settings s;
    s.bind(1, "platformer");
    CHECK(s.packFor(2) == nullptr);
}

TEST(settings_rebinding_a_game_replaces_its_pack) {
    Settings s;
    s.bind(7, "platformer");
    s.bind(7, "full");
    CHECK_EQ(s.gameCount, 1);
    CHECK(std::string(s.packFor(7)) == "full");
}

TEST(settings_keep_the_newest_games_when_the_table_is_full) {
    Settings s;
    for (uint64_t t = 1; t <= static_cast<uint64_t>(kMaxGames) + 5; t++) s.bind(t, "full");
    CHECK_EQ(s.gameCount, kMaxGames);
    CHECK(s.packFor(kMaxGames + 5) != nullptr);  // newest kept
    CHECK(s.packFor(1) == nullptr);              // oldest dropped
}

TEST(settings_accept_hand_written_colour_and_clamp_opacity) {
    Settings s;
    parseSettings("[theme]\ncolor = #f80\nopacity=99\nface_colors=1\n", s);
    CHECK_EQ(s.theme.r, 15);
    CHECK_EQ(s.theme.g, 8);
    CHECK_EQ(s.theme.b, 0);
    CHECK_EQ(s.theme.opacity, kMaxOpacity);
    CHECK(s.theme.faceColors);
    parseSettings("[theme]\nopacity=0\n", s);
    CHECK_EQ(s.theme.opacity, kMinOpacity);
}

TEST(settings_ignore_garbage) {
    Settings s;
    parseSettings("[theme]\ncolor=banana\nopacity=\n[games]\nnot-a-title=full\n0100152000022000=\n\n[general\nprofile=\n", s);
    CHECK(s.theme == Theme{});
    CHECK_EQ(s.gameCount, 0);
    CHECK(std::string(s.defaultPack) == "full");
}

TEST(settings_key_only_counts_in_its_own_section) {
    Settings s;
    parseSettings("[games]\nopacity=2\n[theme]\nprofile=racing\n", s);
    CHECK_EQ(s.theme.opacity, 4);
    CHECK(std::string(s.defaultPack) == "full");
}

// ---- Ultrahand's config ----

TEST(combo_parses_ultrahand_key_names) {
    CHECK_EQ(parseCombo("ZL+ZR+DDOWN"), BtnZL | BtnZR | BtnDown);
    CHECK_EQ(parseCombo("L+DDOWN+RS"), BtnL | BtnDown | BtnStickR);
    CHECK_EQ(parseCombo(" zl + zr + dup "), BtnZL | BtnZR | BtnUp);
    CHECK_EQ(parseCombo("PLUS+MINUS"), BtnPlus | BtnMinus);
}

TEST(combo_with_a_key_we_cannot_press_is_disabled) {
    CHECK_EQ(parseCombo("ZL+SL"), 0);
    CHECK_EQ(parseCombo(""), 0);
    CHECK_EQ(parseCombo("ZL++ZR"), 0);
}

TEST(ini_lookup_respects_sections) {
    const char* text = "[tesla]\nkey_combo=L+DDOWN+RS\n\n[ultrahand]\ndefault_lang = en\nkey_combo = ZL+ZR+DDOWN\r\n[other]\nkey_combo=A\n";
    char out[64];
    CHECK(findIniValue(text, "ultrahand", "key_combo", out, sizeof(out)));
    CHECK(std::string(out) == "ZL+ZR+DDOWN");
    CHECK(!findIniValue(text, "ultrahand", "missing", out, sizeof(out)));
    CHECK(out[0] == 0);
    CHECK(!findIniValue(text, "nosuch", "key_combo", out, sizeof(out)));
}

// ---- what 0.2 left on the card ----

TEST(legacy_profile_moves_the_controls_of_the_standard_pad) {
    Layout l = defaultLayout();
    // A file as 0.2 wrote it.
    parseLegacyProfile("; TouchPad profile. Screen is 1280x720.\nname=Все кнопки\nZL=399,419,78,46,1\nA=1200,254,52,52,1\n"
                       "LS=80,560,40,40,0\nstick_l=173,580,105,1\nstick_r=1125,569,105,0\ndpad=171,303,105,1\n",
                       l);
    CHECK_EQ(button(l, BtnZL)->cx, 399);
    CHECK_EQ(button(l, BtnZL)->cy, 419);
    CHECK_EQ(button(l, BtnZL)->rx, 78);
    CHECK_EQ(button(l, BtnZL)->ry, 46);
    CHECK_EQ(button(l, BtnA)->cy, 254);
    CHECK(!button(l, BtnStickL)->visible);
    CHECK_EQ(button(l, BtnB)->cx, 1110);  // not in the file: stays where it was
    CHECK_EQ(l.stickL.cx, 173);
    CHECK_EQ(l.stickL.cy, 580);
    CHECK(l.stickL.visible && !l.stickR.visible);
    CHECK_EQ(l.dpad.cx, 171);
    CHECK_EQ(l.dpad.cy, 303);
    CHECK_EQ(l.buttonCount, 14);
}

TEST(legacy_profile_ignores_what_it_cannot_read) {
    Layout l = defaultLayout();
    const Layout before = l;
    parseLegacyProfile("WHAT=1,2,3\nB=oops\nA=1,2\nstick_l=5,5\n[pack]\nbtn=A | 100,100,40\n", l);
    CHECK(l == before);
}

TEST(settings_from_0_2_are_recognised_and_new_ones_are_not) {
    Settings old;
    parseSettings("[general]\nprofile=dpad\n\n[games]\n0100152000022000=racing\n", old);
    CHECK(old.legacy);
    CHECK(std::string(old.defaultPack) == "dpad");

    char text[2048];
    CHECK(writeSettings(old, text, sizeof(text)) > 0);
    Settings fresh;
    parseSettings(text, fresh);
    CHECK(!fresh.legacy);  // once rewritten it is a 0.3 file
    CHECK(std::string(fresh.defaultPack) == "dpad");

    Settings none;
    parseSettings("", none);
    CHECK(!none.legacy);
}

TEST(settings_can_forget_a_game) {
    Settings s;
    s.bind(1, "full");
    s.bind(2, "dpad");
    s.bind(3, "minimal");
    s.unbind(2);
    CHECK_EQ(s.gameCount, 2);
    CHECK(s.packFor(2) == nullptr);
    CHECK(std::string(s.packFor(1)) == "full" && std::string(s.packFor(3)) == "minimal");
    s.unbind(99);  // unknown: nothing happens
    CHECK_EQ(s.gameCount, 2);
}

// ---- the shipped game packs as the player sees them ----

namespace {

const Pack* shipped(const char* id) {
    PackStore& s = emptyStore();
    addBuiltinPacks(s);
    const int at = s.find(id);
    return at < 0 ? nullptr : &s.items[at];
}

const ButtonDef* withGlyph(const Layout& l, Glyph glyph) {
    for (int i = 0; i < l.buttonCount; i++)
        if (l.buttons[i].glyph == glyph) return &l.buttons[i];
    return nullptr;
}

}  // namespace

TEST(game_pack_buttons_stay_on_screen_and_off_each_other) {
    for (const char* id : {"mk8d", "lm3", "burnout"}) {
        const Pack* p = shipped(id);
        CHECK(p != nullptr);
        if (!p) continue;
        for (int at = 0; at < p->layoutCount; at++) {
            const Layout& l = p->layouts[at].layout;
            for (int i = 0; i < l.buttonCount; i++) {
                const ButtonDef& a = l.buttons[i];
                // Room for the rim around the button.
                CHECK(a.cx - a.rx >= 4 && a.cx + a.rx <= kScreenW - 4 && a.cy - a.ry >= 4 && a.cy + a.ry <= kScreenH - 4);
                for (int j = i + 1; j < l.buttonCount; j++) {
                    const ButtonDef& b = l.buttons[j];
                    const bool apartX = std::abs(a.cx - b.cx) >= a.rx + b.rx + 8;
                    const bool apartY = std::abs(a.cy - b.cy) >= a.ry + b.ry + 8;
                    const long dx = a.cx - b.cx, dy = a.cy - b.cy, reach = a.rx + b.rx + 8;
                    const bool round = a.shape == Shape::Circle && b.shape == Shape::Circle;
                    const bool apart = round ? dx * dx + dy * dy >= reach * reach : (apartX || apartY);
                    if (!apart) std::printf("  %s [%s]: buttons %d and %d crowd each other\n", id, p->layouts[at].id, i, j);
                    CHECK(apart);
                }
                // Nothing sits on the overlay's own menu button.
                const long mx = a.cx - l.menu.cx, my = a.cy - l.menu.cy, clear = a.rx + l.menu.r + 8;
                CHECK(std::abs(mx) >= clear || std::abs(my) >= a.ry + l.menu.r + 8 || mx * mx + my * my >= clear * clear);
            }
        }
    }
}

TEST(every_pictogram_has_a_name_packs_can_use) {
    for (int g = kFirstIcon; g < kFirstIcon + kIconCount; g++) {
        const char* name = glyphName(static_cast<Glyph>(g));
        CHECK(name[0] != 0);
        std::string text = std::string("[main]\nbtn=A | 100,100,40 | icon=") + name + "\n";
        Pack& p = scratch();
        PackReport report;
        CHECK(parsePack(text.c_str(), p, &report));
        CHECK_EQ(report.badLines, 0);
        CHECK(p.layouts[0].layout.buttons[0].glyph == static_cast<Glyph>(g));
    }
    CHECK(kIconCount >= 30);
}

// ---- what the game says about itself ----

namespace {

const char kStatePack[] =
    "[pack]\n"
    "name=States\n"
    "start=walk\n"
    "[walk]\n"
    "when=!vehicle\n"
    "btn=A | 100,100,40\n"
    "[drive]\n"
    "when=vehicle | entering\n"
    "btn=B | 100,100,40\n"
    "[fly]\n"
    "when=plane & !pause | heli & !pause\n"
    "btn=X | 100,100,40\n"
    "[pause]\n"
    "when=pause\n"
    "btn=Y | 100,100,40\n"
    "[byhand]\n"
    "btn=L | 100,100,40\n"
    "[state]\n"       // may come after the layouts that use it
    "vehicle=0x1\n"
    "plane=2\n"
    "heli=0x4\n"
    "entering=0x40\n"
    "pause=0x10000\n";

}  // namespace

TEST(pack_names_the_bits_of_the_games_state) {
    Pack& p = scratch();
    PackReport report;
    CHECK(parsePack(kStatePack, p, &report));
    CHECK_EQ(report.badLines, 0);
    CHECK_EQ(p.flagCount, 5);
    CHECK_EQ(p.flagMask("vehicle"), 1);
    CHECK_EQ(p.flagMask("plane"), 2);
    CHECK_EQ(p.flagMask("pause"), 0x10000);
    CHECK_EQ(p.flagMask("nope"), 0);
    CHECK_EQ(p.layoutCount, 5);  // [state] is not a layout
    CHECK(p.findLayout("state") < 0);
    CHECK(p.followsGameState());
}

TEST(layout_rule_reads_flags_that_must_be_on_or_off) {
    Pack& p = scratch();
    parsePack(kStatePack, p);
    const WhenRule& walk = p.layouts[0].when;
    CHECK_EQ(walk.terms, 1);
    CHECK(walk.matches(0) && walk.matches(0x10000) && !walk.matches(1));
    const WhenRule& drive = p.layouts[1].when;
    CHECK_EQ(drive.terms, 2);
    CHECK(drive.matches(1) && drive.matches(0x40) && !drive.matches(0) && !drive.matches(2));
    const WhenRule& fly = p.layouts[2].when;
    CHECK(fly.matches(2) && fly.matches(4 | 1) && !fly.matches(2 | 0x10000) && !fly.matches(1));
    CHECK_EQ(p.layouts[4].when.terms, 0);  // no rule: only by hand
}

TEST(the_last_layout_whose_rule_matches_is_the_one) {
    Pack& p = scratch();
    parsePack(kStatePack, p);
    CHECK_EQ(p.layoutFor(0), 0);                 // on foot
    CHECK_EQ(p.layoutFor(1), 1);                 // in a car
    CHECK_EQ(p.layoutFor(0x40), 1);              // walking up to one
    CHECK_EQ(p.layoutFor(1 | 2), 2);             // a plane is a vehicle too: the later layout wins
    CHECK_EQ(p.layoutFor(1 | 0x10000), 3);       // paused in a car
    CHECK_EQ(p.layoutFor(1 | 2 | 0x10000), 3);
}

TEST(layout_rule_with_a_mistake_is_reported_and_leaves_the_layout_manual) {
    Pack& p = scratch();
    PackReport report;
    CHECK(parsePack("[state]\ncar=1\nbig=0x100000000\n=3\n[a]\nwhen=car & truck\nbtn=A | 100,100,40\n[b]\nwhen=car |\nbtn=B | 100,100,40\n"
                    "[c]\nwhen=!car\nbtn=X | 100,100,40\n",
                    p, &report));
    CHECK_EQ(p.flagCount, 1);
    CHECK_EQ(report.badLines, 4);  // the flag that does not fit, the nameless one, and the two broken rules
    CHECK_EQ(p.layouts[0].when.terms, 0);
    CHECK_EQ(p.layouts[1].when.terms, 0);
    CHECK_EQ(p.layouts[2].when.terms, 1);
}

TEST(pack_without_rules_does_not_follow_the_game) {
    Pack& p = scratch();
    parsePack(kSample, p);
    CHECK(!p.followsGameState());
    CHECK_EQ(p.layoutFor(0), -1);
    CHECK_EQ(p.layoutFor(0xFFFFFFFF), -1);
}

TEST(state_flags_and_rules_survive_a_save_and_load) {
    Pack& p = scratch();
    parsePack(kStatePack, p);
    static char text[16 * 1024];
    CHECK(writePack(p, text, sizeof(text)) > 0);
    static Pack q;
    std::strcpy(q.id, "test");
    PackReport report;
    CHECK(parsePack(text, q, &report));
    CHECK_EQ(report.badLines, 0);
    CHECK_EQ(q.flagCount, p.flagCount);
    for (uint32_t state : {0u, 1u, 2u, 3u, 4u, 0x40u, 0x10000u, 0x10001u, 0x10006u}) CHECK_EQ(q.layoutFor(state), p.layoutFor(state));
    CHECK_EQ(q.layouts[4].when.terms, 0);
}

TEST(a_pack_with_rules_follows_the_flags_a_mod_reports) {
    PackStore& store = emptyStore();
    const Pack* p = &store.items[addRoamPack(store)];
    CHECK(p->followsGameState());
    CHECK_EQ(p->flagMask("vehicle"), 0x1);
    CHECK_EQ(p->flagMask("plane"), 0x2);
    CHECK_EQ(p->flagMask("heli"), 0x4);
    CHECK_EQ(p->flagMask("entering"), 0x40);
    CHECK_EQ(p->flagMask("phone"), 0x400);
    CHECK_EQ(p->flagMask("pause"), 0x10000);
    CHECK_EQ(p->flagMask("alive"), 0x40000000);
    const uint32_t alive = 0x40000000 | 0x100000;
    CHECK_EQ(p->layoutFor(alive), p->findLayout("foot"));
    CHECK_EQ(p->layoutFor(alive | 0x1), p->findLayout("car"));
    CHECK_EQ(p->layoutFor(alive | 0x40), p->findLayout("car"));
    CHECK_EQ(p->layoutFor(alive | 0x1 | 0x10), p->findLayout("car"));   // a bike
    CHECK_EQ(p->layoutFor(alive | 0x1 | 0x2), p->findLayout("air"));
    CHECK_EQ(p->layoutFor(alive | 0x1 | 0x4), p->findLayout("air"));
    CHECK_EQ(p->layoutFor(alive | 0x1 | 0x10000), p->findLayout("menu"));
    CHECK_EQ(p->layoutFor(alive | 0x1 | 0x400), p->findLayout("car"));  // a ringing phone changes nothing
    CHECK(p->layouts[p->findLayout("menu")].holds);
    // The game freezes its scripts in the pause menu: the overlay marks the silent report itself.
    CHECK_EQ(p->flagMask("stalled"), 0x80000000u);
    CHECK_EQ(p->layoutFor(alive | 0x1 | 0x80000000u), p->findLayout("menu"));
    const Pack* kart = shipped("mk8d");
    CHECK(kart && !kart->followsGameState());
}

TEST(luigis_mansion_pack_aims_the_poltergust_with_the_thumb_that_holds_the_button) {
    const Pack* p = shipped("lm3");
    CHECK(p != nullptr);
    if (!p) return;
    CHECK(p->madeFor(0x0100DCA0064A6000ull));
    const Layout& play = p->layouts[p->findLayout("play")].layout;
    CHECK_EQ(p->start, p->findLayout("play"));
    // Vacuum, blow, flash, plunger and dark light all turn Luigi while held.
    for (Glyph g : {Glyph::Suck, Glyph::Blow, Glyph::Bulb, Glyph::Aim, Glyph::Eye}) {
        const ButtonDef* b = withGlyph(play, g);
        CHECK(b != nullptr);
        if (b) CHECK(b->drag == Drag::StickR);
    }
    CHECK_EQ(withGlyph(play, Glyph::Suck)->bit, BtnZR);
    CHECK_EQ(withGlyph(play, Glyph::Blow)->bit, BtnZL);
    CHECK_EQ(withGlyph(play, Glyph::Bulb)->bit, BtnA);
    CHECK_EQ(withGlyph(play, Glyph::Aim)->bit, BtnY);
    CHECK_EQ(withGlyph(play, Glyph::Eye)->bit, BtnX);
    CHECK_EQ(withGlyph(play, Glyph::Fire)->bit, BtnZL | BtnZR);  // burst
    CHECK_EQ(withGlyph(play, Glyph::Ghost)->bit, BtnStickR);     // Gooigi
    CHECK(withGlyph(play, Glyph::Run)->press == Press::Toggle);
    CHECK_EQ(withGlyph(play, Glyph::Run)->bit, BtnB);
    // A Nintendo game: A confirms, B goes back.
    const Layout& menu = p->layouts[p->findLayout("menu")].layout;
    CHECK_EQ(withGlyph(menu, Glyph::Ok)->bit, BtnA);
    CHECK_EQ(withGlyph(menu, Glyph::Back)->bit, BtnB);
}

TEST(burnout_pack_keeps_the_gas_down_under_boost_and_handbrake) {
    const Pack* p = shipped("burnout");
    CHECK(p != nullptr);
    if (!p) return;
    CHECK(p->madeFor(0x0100DBF01000A000ull));
    const Layout& drive = p->layouts[p->findLayout("drive")].layout;
    CHECK_EQ(withGlyph(drive, Glyph::Gas)->bit, BtnZR);
    // One thumb cannot hold two buttons, so these press the gas themselves.
    CHECK_EQ(withGlyph(drive, Glyph::Bolt)->bit, BtnZR | BtnB);
    CHECK_EQ(withGlyph(drive, Glyph::Handbrake)->bit, BtnZR | BtnY);
    CHECK_EQ(withGlyph(drive, Glyph::Brake)->bit, BtnZL);
    CHECK_EQ(withGlyph(drive, Glyph::Flag)->bit, BtnZL | BtnZR);  // start an event at the lights
    CHECK_EQ(withGlyph(drive, Glyph::Fire)->bit, BtnL | BtnR);    // Showtime
}
