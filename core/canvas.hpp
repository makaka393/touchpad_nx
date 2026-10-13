// Software rasteriser for the overlay layer: anti-aliased shapes on an RGBA4444 buffer.
// Callers work in logical 1280x720 coordinates; the buffer may be any resolution.
#pragma once
#include "types.hpp"

namespace tp {

struct Color {
    uint8_t r, g, b, a;  // 4 bits per channel, straight (non-premultiplied) alpha
};

struct Rect {
    int x0, y0, x1, y1;  // half-open, logical coordinates
    bool intersects(const Rect& o) const { return x0 < o.x1 && o.x0 < x1 && y0 < o.y1 && o.y0 < y1; }
};

class Canvas {
public:
    // stridePixels: distance between rows, in pixels.
    Canvas(uint16_t* pixels, int width, int height, int stridePixels);

    int width() const { return m_w; }
    int height() const { return m_h; }
    uint16_t pixel(int fx, int fy) const { return m_px[fy * m_stride + fx]; }
    float scale() const { return m_scale; }  // framebuffer pixels per logical unit

    // Store colours already multiplied by their alpha. The console's display layer wants
    // that: given plain colours it shows every faint white as solid white.
    void setPremultiplied(bool on) { m_premultiplied = on; }

    // Restricts all drawing (including clear) to a logical rectangle.
    void setClip(const Rect& logical);
    void resetClip();
    void clear();  // makes the clip region fully transparent

    void fillCircle(float cx, float cy, float r, Color c);
    void strokeCircle(float cx, float cy, float r, float width, Color c);
    void fillRoundRect(float cx, float cy, float hw, float hh, float radius, Color c);
    void strokeRoundRect(float cx, float cy, float hw, float hh, float radius, float width, Color c);
    // Plus-shaped union of a horizontal and a vertical bar, for the d-pad.
    void fillCross(float cx, float cy, float armLen, float armHalfWidth, float radius, Color c);
    void strokeCross(float cx, float cy, float armLen, float armHalfWidth, float radius, float width, Color c);
    void line(float x0, float y0, float x1, float y1, float width, Color c);  // round caps

    // One framebuffer pixel with 0..255 coverage; for glyph bitmaps. Honours the clip.
    void blendPixel(int fx, int fy, uint8_t coverage, Color c);

    // Paints a square coverage mask (size x size pixels, 4 bits each, two per byte with the
    // low nibble first) scaled into the square of half-width `half` around (cx, cy).
    void drawMask(const uint8_t* mask, int size, float cx, float cy, float half, Color c);

private:
    void put(uint16_t& dst, Color c, float coverage) const;

    template <typename Sdf>
    void shade(float bx0, float by0, float bx1, float by1, Color c, Sdf sdf);

    uint16_t* m_px;
    int m_w, m_h, m_stride;
    float m_scale;                       // framebuffer pixels per logical unit
    bool m_premultiplied = false;
    int m_cx0, m_cy0, m_cx1, m_cy1;      // clip, framebuffer pixels, half-open
};

}  // namespace tp
