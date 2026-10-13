#include "tinytest.hpp"
#include "../core/font.hpp"

#include <cstdio>
#include <vector>

using namespace tp;

namespace {

// Any TrueType font with Latin and Cyrillic will do for the host; the console uses its own.
const std::vector<unsigned char>& fontFile() {
    static std::vector<unsigned char> data = [] {
        std::vector<unsigned char> d;
        for (const char* path : {"/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/noto/NotoSans-Regular.ttf",
                                 "/usr/share/fonts/liberation/LiberationSans-Regular.ttf"}) {
            if (FILE* f = std::fopen(path, "rb")) {
                std::fseek(f, 0, SEEK_END);
                d.resize(static_cast<size_t>(std::ftell(f)));
                std::fseek(f, 0, SEEK_SET);
                if (std::fread(d.data(), 1, d.size(), f) != d.size()) d.clear();
                std::fclose(f);
                if (!d.empty()) break;
            }
        }
        return d;
    }();
    return data;
}

struct Page {
    std::vector<uint16_t> px = std::vector<uint16_t>(640 * 360, 0);
    Canvas canvas{px.data(), 640, 360, 640};
    Font font;

    Page() { font.load(fontFile().data()); }

    // Number of painted pixels in a framebuffer rectangle.
    int ink(int x0, int y0, int x1, int y1) const {
        int n = 0;
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++) n += (canvas.pixel(x, y) >> 12) > 0;
        return n;
    }
    int inkAll() const { return ink(0, 0, 640, 360); }
};

constexpr Color kWhite{15, 15, 15, 15};

}  // namespace

TEST(host_font_is_available_for_these_tests) {
    CHECK(fontFile().size() > 10000);
}

TEST(font_is_not_ready_until_loaded) {
    Font f;
    CHECK(!f.ready());
    Page p;
    CHECK(p.font.ready());
}

TEST(unloaded_font_draws_nothing) {
    Page p;
    Font blank;
    CHECK_EQ(blank.draw(p.canvas, 100, 100, 40, "A", kWhite), 0);
    CHECK_EQ(p.inkAll(), 0);
}

TEST(text_sits_on_its_baseline_to_the_right_of_x) {
    Page p;
    p.font.draw(p.canvas, 200, 200, 60, "H", kWhite);  // pixel origin (100,100), 30 px line height
    CHECK(p.ink(100, 75, 125, 101) > 40);   // the letter itself
    CHECK_EQ(p.ink(0, 0, 99, 360), 0);      // nothing left of x
    CHECK_EQ(p.ink(0, 102, 640, 360), 0);   // nothing below the baseline for a capital
    CHECK_EQ(p.ink(0, 0, 640, 70), 0);      // nothing above the line
}

TEST(draw_returns_the_same_width_it_measures) {
    Page p;
    const float measured = p.font.width(p.canvas, 40, "Profile");
    const float drawn = p.font.draw(p.canvas, 100, 100, 40, "Profile", kWhite);
    CHECK(measured > 60 && measured < 200);
    CHECK(drawn == measured);
}

TEST(cyrillic_text_is_measured_and_drawn_per_character) {
    Page p;
    const float six = p.font.width(p.canvas, 40, "Профил");
    const float seven = p.font.width(p.canvas, 40, "Профиль");
    CHECK(six > 60);
    CHECK(seven > six + 5);
    p.font.draw(p.canvas, 100, 200, 40, "Профиль", kWhite);
    CHECK(p.inkAll() > 200);
}

TEST(width_scales_with_size) {
    Page p;
    const float small = p.font.width(p.canvas, 30, "Раскладка");
    const float big = p.font.width(p.canvas, 60, "Раскладка");
    CHECK(big > small * 1.8f && big < small * 2.2f);
}

TEST(text_respects_the_clip) {
    Page p;
    p.canvas.setClip(Rect{0, 0, 100, 720});
    p.font.draw(p.canvas, 200, 200, 60, "HHHH", kWhite);
    CHECK_EQ(p.inkAll(), 0);
}

TEST(text_running_off_the_screen_is_cut_not_wrapped) {
    Page p;  // AddressSanitizer catches any stray write
    p.font.draw(p.canvas, 1200, 700, 80, "WWWWWWWWWWWW", kWhite);
    p.font.draw(p.canvas, -300, 20, 80, "WWWWWWWWWWWW", kWhite);
    CHECK_EQ(p.ink(0, 0, 200, 300) - p.ink(0, 0, 200, 60), 0);  // nothing wrapped onto later rows
}

TEST(malformed_utf8_does_not_break_drawing) {
    Page p;
    p.font.draw(p.canvas, 100, 100, 40, "A\xFF\xD0", kWhite);  // stray byte, then a cut-off character
    p.font.draw(p.canvas, 100, 200, 40, "\xE2\x82", kWhite);
    CHECK(p.inkAll() > 20);  // the valid letter still appears
}
