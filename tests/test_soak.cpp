#include "tinytest.hpp"

#include <cstring>
#include "../core/render.hpp"
#include "roam_pack.hpp"

#include <cstdio>
#include <vector>

#include "../core/text.hpp"

using namespace tp;

namespace {

// Small deterministic generator: the run is identical every time.
struct Rng {
    uint32_t state = 0x1234567u;
    uint32_t next() {
        state = state * 1664525u + 1013904223u;
        return state >> 8;
    }
    int below(int n) { return static_cast<int>(next() % static_cast<uint32_t>(n)); }
};

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

}  // namespace

// Thousands of random fingers through every mode (menu, editor, pack and layout switches, saves),
// drawing each resulting frame. AddressSanitizer and UBSan watch for stray writes; the
// checks below watch for state that should be impossible.
TEST(random_touch_storm_keeps_everything_sane) {
    static App app;
    addRoamPack(app.packs());
    app.setGame(0);
    std::vector<unsigned char> fontData = loadFont();
    Font font;
    font.load(fontData.data());
    std::vector<uint16_t> px(640 * 360, 0);
    Canvas canvas(px.data(), 640, 360, 640);
    Renderer renderer(canvas, &font);

    // Places a random finger is likely to be useful: menu button, menu rows, tools, controls.
    // The last six are buttons of the game packs that switch layouts, latch or steer a stick.
    const int hotX[] = {640, 640, 500, 900, 480, 600, 760, 1202, 1110, 170, 330, 950, 92, 1188, 1218, 772, 1008, 1168, 70, 640};
    const int hotY[] = {46, 200, 300, 260, 570, 630, 630, 300, 392, 300, 565, 565, 62, 62, 330, 44, 642, 596, 56, 672};
    constexpr int kHot = sizeof(hotX) / sizeof(hotX[0]);
    // Two games without a pack, and the two that have one.
    const uint64_t games[] = {0x0100000000001000ull, 0x0100000000001001ull, kRoam, 0x0100152000022000ull};

    Rng rng;
    TouchPoint fingers[10];
    int count = 0;
    uint64_t now = 1000;
    int framesDrawn = 0, modesSeen = 0, layoutSwitches = 0, lastLayout = 0;
    bool touchLeftToGame = false;

    for (int step = 0; step < 30000; step++) {
        // Mutate the finger set: lift one, land one, or move them.
        const int action = rng.below(10);
        if (action < 2 && count > 0) {
            fingers[rng.below(count)] = fingers[count - 1];
            count--;
        } else if (action < 4 && count < 10) {
            const int h = rng.below(kHot);
            const bool hot = rng.below(3) != 0;
            fingers[count++] = TouchPoint{static_cast<uint32_t>(step), hot ? hotX[h] + rng.below(21) - 10 : rng.below(1280),
                                          hot ? hotY[h] + rng.below(21) - 10 : rng.below(720)};
        } else {
            for (int i = 0; i < count; i++) {
                fingers[i].x += rng.below(61) - 30;
                fingers[i].y += rng.below(61) - 30;
            }
        }
        if (rng.below(400) == 0) count = 0;                                   // everything lifts now and then

        // Now and then a deliberate, clean tap on something meaningful, so the storm actually
        // walks through the menu and the editor instead of only brushing past them.
        if (rng.below(25) == 0) {
            Rect target{610, 16, 670, 76};  // menu button
            const int pick = rng.below(4);
            if (pick == 1) target = ui::menuRow(rng.below(kMenuRows - 1));  // every row but "leave"
            if (pick == 2) target = ui::menuArrow(rng.below(2) ? RowPack : RowColor, rng.below(2));
            if (pick == 3) target = ui::toolButton(rng.below(kEditTools));
            const TouchPoint tapAt{0xFFFF0000u + static_cast<uint32_t>(step), (target.x0 + target.x1) / 2, (target.y0 + target.y1) / 2};
            app.update(nullptr, 0, now += 10);
            app.update(&tapAt, 1, now += 10);
            app.update(&tapAt, 1, now += 40);
            app.update(nullptr, 0, now += 10);
            count = 0;
        }
        if (rng.below(1500) == 0) app.setGame(games[rng.below(4)]);  // another game starts
        // A game mod reporting (or falling silent) at any moment, with any mix of flags.
        if (rng.below(30) == 0) {
            const uint32_t flags[] = {0x1, 0x2, 0x4, 0x40, 0x400, 0x10000, 0x40000000};
            uint32_t bits = 0;
            for (uint32_t flag : flags) bits |= rng.below(3) == 0 ? flag : 0;
            app.setGameState(rng.below(5) != 0, bits);
        }

        now += 4 + rng.below(40);
        app.update(fingers, count, now);
        app.takeExitRequest();
        app.takeSettingsDirty();
        app.takePackToSave();

        Scene scene;
        app.fillScene(scene);
        modesSeen |= 1 << static_cast<int>(scene.mode);
        framesDrawn += renderer.draw(scene);

        if (app.currentLayout() != lastLayout) layoutSwitches++;
        lastLayout = app.currentLayout();
        touchLeftToGame |= !app.blockGameTouch() && scene.mode == Mode::Play;

        // Invariants.
        if (app.currentLayout() < 0 || app.currentLayout() >= app.packs().items[app.currentPack()].layoutCount) { CHECK(false); break; }
        // The game may only see touches during play (on a layout that says so) or with the pad hidden.
        if (!app.blockGameTouch() && scene.mode != Mode::Play && scene.mode != Mode::Hidden) { CHECK(false); break; }
        if (app.packs().count < 1 || app.packs().count > kMaxPacks) { CHECK(false); break; }
        if (app.currentPack() < 0 || app.currentPack() >= app.packs().count) { CHECK(false); break; }
        if (app.settings().gameCount < 0 || app.settings().gameCount > kMaxGames) { CHECK(false); break; }
        if (app.settings().theme.opacity < kMinOpacity || app.settings().theme.opacity > kMaxOpacity) { CHECK(false); break; }
        bool inBounds = true;
        const Layout& l = scene.layout;
        for (int i = 0; i < l.buttonCount; i++) {
            const ButtonDef& b = l.buttons[i];
            inBounds &= b.cx - b.rx >= 0 && b.cx + b.rx <= kScreenW && b.cy - b.ry >= 0 && b.cy + b.ry <= kScreenH;
            inBounds &= b.rx >= 20 && b.ry >= 20;
        }
        inBounds &= l.stickL.cx - l.stickL.baseRadius >= 0 && l.stickL.cx + l.stickL.baseRadius <= kScreenW;
        inBounds &= l.stickR.cy - l.stickR.baseRadius >= 0 && l.stickR.cy + l.stickR.baseRadius <= kScreenH;
        inBounds &= l.dpad.cx - l.dpad.armLen >= 0 && l.dpad.cy + l.dpad.armLen <= kScreenH;
        if (!inBounds) { CHECK(inBounds); break; }
        // Outside play the pad must never leak input into the game.
        if (scene.mode != Mode::Play && !(app.pad() == PadState{})) { CHECK(false); break; }
    }

    CHECK_EQ(modesSeen, 0b1111);  // the storm really visited play, hidden, menu and editor
    CHECK(framesDrawn > 1000);
    CHECK(layoutSwitches > 20);  // and really moved between the layouts of the game packs
    CHECK(touchLeftToGame);
}

// Whatever was saved must load back to the same thing, for every pack that ships.
TEST(every_pack_survives_a_save_and_load) {
    static PackStore store;
    store.count = 0;
    addBuiltinPacks(store);
    static Pack loaded;
    static char text[24 * 1024], again[24 * 1024];
    for (int i = 0; i < store.count; i++) {
        const Pack& saved = store.items[i];
        CHECK(writePack(saved, text, sizeof(text)) > 0);
        copyText(loaded.id, sizeof(loaded.id), saved.id);
        PackReport report;
        CHECK(parsePack(text, loaded, &report));
        CHECK_EQ(report.badLines, 0);
        CHECK_EQ(loaded.layoutCount, saved.layoutCount);
        CHECK_EQ(loaded.start, saved.start);
        CHECK_EQ(loaded.gameCount, saved.gameCount);

        for (int at = 0; at < saved.layoutCount && at < loaded.layoutCount; at++) {
            const Layout& a = saved.layouts[at].layout;
            const Layout& b = loaded.layouts[at].layout;
            CHECK_EQ(b.buttonCount, a.buttonCount);
            CHECK_EQ(b.touchToGame, a.touchToGame);
            for (int k = 0; k < a.buttonCount && k < b.buttonCount; k++) {
                CHECK_EQ(b.buttons[k].bit, a.buttons[k].bit);
                CHECK_EQ(b.buttons[k].cx, a.buttons[k].cx);
                CHECK_EQ(b.buttons[k].cy, a.buttons[k].cy);
                CHECK_EQ(b.buttons[k].rx, a.buttons[k].rx);
                CHECK_EQ(b.buttons[k].ry, a.buttons[k].ry);
                CHECK_EQ(b.buttons[k].visible, a.buttons[k].visible);
                CHECK(b.buttons[k].glyph == a.buttons[k].glyph);
                CHECK(b.buttons[k].press == a.buttons[k].press);
                CHECK(b.buttons[k].drag == a.buttons[k].drag);
                CHECK_EQ(b.buttons[k].gotoLayout, a.buttons[k].gotoLayout);
                CHECK(std::strcmp(b.buttons[k].label, a.buttons[k].label) == 0);
            }
            CHECK_EQ(b.stickL.visible, a.stickL.visible);
            CHECK_EQ(b.stickR.visible, a.stickR.visible);
            CHECK_EQ(b.stickL.floating, a.stickL.floating);
            CHECK_EQ(b.stickR.hint, a.stickR.hint);
            CHECK_EQ(b.dpad.visible, a.dpad.visible);
            CHECK_EQ(b.stickL.zoneRadius, a.stickL.zoneRadius);
            CHECK_EQ(b.stickL.throwRadius, a.stickL.throwRadius);
            CHECK_EQ(b.dpad.armHalfWidth, a.dpad.armHalfWidth);
            CHECK_EQ(b.dpad.zoneRadius, a.dpad.zoneRadius);
        }

        // And the text itself is stable: writing what was read gives the same file.
        CHECK(writePack(loaded, again, sizeof(again)) > 0);
        CHECK(std::strcmp(text, again) == 0);
    }
}
