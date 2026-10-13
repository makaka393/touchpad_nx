#include "tinytest.hpp"
#include "../core/canvas.hpp"

#include <vector>

using namespace tp;

namespace {

// 640x360 buffer = half the logical resolution, as on the console: logical (100,100) is pixel (50,50).
struct Surface {
    std::vector<uint16_t> px = std::vector<uint16_t>(640 * 360, 0);
    Canvas canvas{px.data(), 640, 360, 640};

    int alpha(int fx, int fy) const { return canvas.pixel(fx, fy) >> 12; }
    int red(int fx, int fy) const { return canvas.pixel(fx, fy) & 0xF; }
};

constexpr Color kWhite{15, 15, 15, 15};
constexpr Color kBlack{0, 0, 0, 15};

}  // namespace

TEST(fill_circle_paints_inside_and_leaves_outside) {
    Surface s;
    s.canvas.fillCircle(100, 100, 20, kWhite);
    CHECK_EQ(s.canvas.pixel(50, 50), 0xFFFF);
    CHECK_EQ(s.canvas.pixel(50, 57), 0xFFFF);  // 7.5 px from the centre, radius is 10 px
    CHECK_EQ(s.canvas.pixel(50, 62), 0);       // 12.5 px away
    CHECK_EQ(s.canvas.pixel(10, 10), 0);
}

TEST(circle_edge_is_antialiased) {
    Surface s;
    s.canvas.fillCircle(100, 100, 21, kWhite);  // 10.5 px radius
    // Pixel (60,50) has its centre exactly on the edge: about half covered.
    CHECK(s.alpha(60, 50) >= 6 && s.alpha(60, 50) <= 9);
    CHECK_EQ(s.red(60, 50), 15);
}

TEST(translucent_colour_keeps_its_rgb_on_empty_background) {
    Surface s;
    s.canvas.fillCircle(100, 100, 20, Color{15, 15, 15, 6});
    CHECK_EQ(s.canvas.pixel(50, 50), 0x6FFF);  // straight alpha: rgb is not scaled by alpha
}

TEST(translucent_black_darkens_opaque_white) {
    Surface s;
    s.canvas.fillCircle(100, 100, 20, kWhite);
    s.canvas.fillCircle(100, 100, 20, Color{0, 0, 0, 8});
    CHECK_EQ(s.alpha(50, 50), 15);
    CHECK_EQ(s.red(50, 50), 7);  // 15 * (1 - 8/15)
}

TEST(opaque_paint_replaces_what_is_below) {
    Surface s;
    s.canvas.fillCircle(100, 100, 20, kWhite);
    s.canvas.fillCircle(100, 100, 20, kBlack);
    CHECK_EQ(s.canvas.pixel(50, 50), 0xF000);
}

TEST(stroke_circle_paints_the_ring_only) {
    Surface s;
    s.canvas.strokeCircle(100, 100, 40, 4, kWhite);  // 20 px radius, 2 px wide
    CHECK_EQ(s.canvas.pixel(70, 50), 0xFFFF);   // centre 20.5 px out: on the ring
    CHECK_EQ(s.canvas.pixel(50, 50), 0);        // hollow centre
    CHECK_EQ(s.canvas.pixel(75, 50), 0);        // beyond the ring
}

TEST(round_rect_fills_body_and_rounds_corners) {
    Surface s;
    s.canvas.fillRoundRect(200, 100, 80, 40, 30, kWhite);  // pixels x 60..140, y 30..70, corner radius 15
    CHECK_EQ(s.canvas.pixel(100, 50), 0xFFFF);
    CHECK_EQ(s.canvas.pixel(61, 50), 0xFFFF);   // left edge, mid height
    CHECK_EQ(s.canvas.pixel(60, 30), 0);        // the square corner is cut away
    CHECK_EQ(s.canvas.pixel(145, 50), 0);
}

TEST(stroke_round_rect_is_hollow) {
    Surface s;
    s.canvas.strokeRoundRect(200, 100, 80, 40, 30, 4, kWhite);
    CHECK_EQ(s.canvas.pixel(100, 50), 0);
    CHECK_EQ(s.canvas.pixel(100, 30), 0xFFFF);  // top edge
}

TEST(cross_covers_both_bars_but_not_the_corners) {
    Surface s;
    s.canvas.fillCross(200, 200, 100, 30, 8, kWhite);  // arms reach 50 px, 15 px half width
    CHECK_EQ(s.canvas.pixel(100, 100), 0xFFFF);  // centre
    CHECK_EQ(s.canvas.pixel(140, 100), 0xFFFF);  // right arm
    CHECK_EQ(s.canvas.pixel(100, 60), 0xFFFF);   // top arm
    CHECK_EQ(s.canvas.pixel(135, 65), 0);        // between arms
}

TEST(cross_outline_has_no_seam_through_the_middle) {
    Surface s;
    s.canvas.strokeCross(200, 200, 100, 30, 8, 4, kWhite);
    CHECK_EQ(s.canvas.pixel(100, 100), 0);       // interior stays empty
    CHECK_EQ(s.canvas.pixel(115, 100), 0);       // x=230: where a naive two-rect outline would draw a line
    CHECK_EQ(s.canvas.pixel(149, 100), 0xFFFF);  // tip of the right arm
}

TEST(line_has_width_and_round_caps) {
    Surface s;
    s.canvas.line(100, 100, 200, 100, 8, kWhite);  // pixels x 50..100 at y 50, 4 px wide
    CHECK_EQ(s.canvas.pixel(75, 50), 0xFFFF);
    CHECK_EQ(s.canvas.pixel(75, 51), 0xFFFF);
    CHECK_EQ(s.canvas.pixel(75, 54), 0);
    CHECK(s.alpha(101, 50) >= 12);                 // cap extends past the end point
    CHECK_EQ(s.canvas.pixel(104, 50), 0);
}

TEST(clip_limits_drawing) {
    Surface s;
    s.canvas.setClip(Rect{0, 0, 100, 720});  // pixels x < 50
    s.canvas.fillCircle(100, 100, 40, kWhite);
    CHECK_EQ(s.canvas.pixel(45, 50), 0xFFFF);
    CHECK_EQ(s.canvas.pixel(50, 50), 0);
    CHECK_EQ(s.canvas.pixel(55, 50), 0);
}

TEST(clear_only_touches_the_clip_region) {
    Surface s;
    s.canvas.fillCircle(100, 100, 40, kWhite);
    s.canvas.setClip(Rect{0, 0, 100, 720});
    s.canvas.clear();
    CHECK_EQ(s.canvas.pixel(45, 50), 0);
    CHECK_EQ(s.canvas.pixel(55, 50), 0xFFFF);
}

TEST(reset_clip_restores_full_surface) {
    Surface s;
    s.canvas.setClip(Rect{0, 0, 100, 720});
    s.canvas.resetClip();
    s.canvas.fillCircle(100, 100, 40, kWhite);
    CHECK_EQ(s.canvas.pixel(55, 50), 0xFFFF);
}

TEST(shapes_partly_off_screen_do_not_write_out_of_bounds) {
    Surface s;  // AddressSanitizer turns any stray write into a failure
    s.canvas.fillCircle(0, 0, 60, kWhite);
    s.canvas.fillCircle(1280, 720, 60, kWhite);
    s.canvas.setClip(Rect{-50, -50, 2000, 2000});
    s.canvas.line(-100, 300, 1400, 400, 10, kWhite);
    s.canvas.clear();
    CHECK_EQ(s.canvas.pixel(0, 0), 0);
    CHECK_EQ(s.canvas.pixel(639, 359), 0);
}

TEST(row_stride_wider_than_the_surface_is_respected) {
    std::vector<uint16_t> px(704 * 360, 0);
    Canvas canvas(px.data(), 640, 360, 704);
    canvas.fillCircle(100, 100, 20, kWhite);
    CHECK_EQ(px[50 * 704 + 50], 0xFFFF);
    CHECK_EQ(px[50 * 640 + 50], 0);
}

// ---- coverage masks (pictograms) ----

namespace {

// An 8x8 mask, 4 bits per pixel: the left half solid, the right half empty.
struct HalfMask {
    uint8_t bytes[32];
    HalfMask() {
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x += 2) bytes[(y * 8 + x) / 2] = x < 4 ? 0xFF : 0x00;
    }
};

}  // namespace

TEST(mask_is_scaled_into_the_square_it_is_given) {
    Surface s;
    HalfMask m;
    s.canvas.drawMask(m.bytes, 8, 200, 200, 40, kWhite);  // logical 160..240, pixels 80..120
    CHECK_EQ(s.canvas.pixel(85, 100), 0xFFFF);   // solid half
    CHECK_EQ(s.canvas.pixel(99, 81), 0xFFFF);
    CHECK_EQ(s.canvas.pixel(105, 100), 0);       // empty half
    CHECK_EQ(s.canvas.pixel(79, 100), 0);        // outside the square
    CHECK_EQ(s.canvas.pixel(85, 121), 0);
}

TEST(mask_shrunk_below_its_own_size_is_averaged_not_sampled) {
    Surface s;
    // Stripes one mask pixel wide: shrunk four times they must come out as an even half tone.
    uint8_t stripes[32];
    for (int i = 0; i < 32; i++) stripes[i] = 0x0F;
    s.canvas.drawMask(stripes, 8, 100, 100, 2, kWhite);  // 8 mask pixels onto 2 framebuffer pixels
    CHECK(s.alpha(49, 49) >= 6 && s.alpha(49, 49) <= 9);
    CHECK(s.alpha(50, 50) >= 6 && s.alpha(50, 50) <= 9);
}

TEST(mask_takes_the_colour_and_respects_the_clip) {
    Surface s;
    HalfMask m;
    s.canvas.setClip(Rect{160, 160, 180, 240});  // only the first 10 pixel columns
    s.canvas.drawMask(m.bytes, 8, 200, 200, 40, Color{15, 0, 0, 15});
    s.canvas.resetClip();
    CHECK_EQ(s.red(85, 100), 15);
    CHECK_EQ(s.canvas.pixel(85, 100) & 0xFF0, 0);  // no green or blue
    CHECK_EQ(s.canvas.pixel(95, 100), 0);          // solid in the mask, but outside the clip
}

TEST(mask_half_off_screen_does_not_write_outside_the_buffer) {
    Surface s;
    HalfMask m;
    // AddressSanitizer is the real judge here.
    s.canvas.drawMask(m.bytes, 8, 0, 0, 40, kWhite);        // only the empty half is on screen
    s.canvas.drawMask(m.bytes, 8, 1280, 720, 40, kWhite);   // a corner of the solid half is
    CHECK_EQ(s.canvas.pixel(5, 5), 0);
    CHECK_EQ(s.canvas.pixel(630, 350), 0xFFFF);
}

// ---- colours multiplied by their alpha, as the console's display layer wants them ----

TEST(premultiplied_faint_white_is_stored_as_a_faint_colour_not_full_white) {
    Surface s;
    s.canvas.setPremultiplied(true);
    s.canvas.fillCircle(100, 100, 20, Color{15, 15, 15, 3});
    CHECK_EQ(s.canvas.pixel(50, 50), 0x3333);  // colour 3 of 15 with alpha 3: adds a fifth of white
}

TEST(premultiplied_opaque_colour_is_unchanged_and_black_only_dims) {
    Surface s;
    s.canvas.setPremultiplied(true);
    s.canvas.fillCircle(100, 100, 20, kWhite);
    CHECK_EQ(s.canvas.pixel(50, 50), 0xFFFF);
    s.canvas.fillCircle(300, 100, 20, Color{0, 0, 0, 6});
    CHECK_EQ(s.canvas.pixel(150, 50), 0x6000);
}

TEST(premultiplied_layers_add_up_without_passing_full) {
    Surface s;
    s.canvas.setPremultiplied(true);
    s.canvas.fillCircle(100, 100, 20, Color{15, 15, 15, 3});
    s.canvas.fillCircle(100, 100, 10, Color{15, 15, 15, 13});  // a legend on the glass
    const int rim = s.canvas.pixel(50, 58), middle = s.canvas.pixel(50, 50);
    CHECK_EQ(rim, 0x3333);
    CHECK((middle & 0xF) >= 13 && (middle & 0xF) <= 15);
    CHECK((middle >> 12) >= 13);
    for (int i = 0; i < 5; i++) s.canvas.fillCircle(100, 100, 10, kWhite);
    CHECK_EQ(s.canvas.pixel(50, 50), 0xFFFF);
}
