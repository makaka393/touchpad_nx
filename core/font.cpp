#define STB_TRUETYPE_IMPLEMENTATION
#include "font.hpp"

#include <cmath>

#include "text.hpp"

namespace tp {

namespace {

// Largest glyph bitmap we rasterise, in framebuffer pixels. Menu text is far smaller.
constexpr int kMaxGlyph = 96;

}  // namespace

bool Font::load(const unsigned char* data) {
    m_ready = data && stbtt_InitFont(&m_info, data, stbtt_GetFontOffsetForIndex(data, 0)) != 0;
    return m_ready;
}

float Font::width(const Canvas& canvas, float size, const char* utf8) const {
    if (!m_ready) return 0;
    const float scale = stbtt_ScaleForPixelHeight(&m_info, size * canvas.scale());
    float pen = 0;
    for (const char* p = utf8; *p;) {
        int advance = 0, bearing = 0;
        stbtt_GetCodepointHMetrics(&m_info, static_cast<int>(nextCodepoint(p)), &advance, &bearing);
        pen += advance * scale;
    }
    return pen / canvas.scale();
}

float Font::draw(Canvas& canvas, float x, float baseline, float size, const char* utf8, Color color) const {
    if (!m_ready) return 0;
    const float scale = stbtt_ScaleForPixelHeight(&m_info, size * canvas.scale());
    const float originX = x * canvas.scale();
    const int originY = static_cast<int>(std::lround(baseline * canvas.scale()));

    static unsigned char bitmap[kMaxGlyph * kMaxGlyph];  // render thread only
    float pen = 0;
    for (const char* p = utf8; *p;) {
        const int cp = static_cast<int>(nextCodepoint(p));
        int advance = 0, bearing = 0;
        stbtt_GetCodepointHMetrics(&m_info, cp, &advance, &bearing);

        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&m_info, cp, scale, scale, &x0, &y0, &x1, &y1);
        const int w = x1 - x0, h = y1 - y0;
        if (w > 0 && h > 0 && w <= kMaxGlyph && h <= kMaxGlyph) {
            stbtt_MakeCodepointBitmap(&m_info, bitmap, w, h, w, scale, scale, cp);
            const int left = static_cast<int>(std::lround(originX + pen)) + x0;
            const int top = originY + y0;
            // Skip glyphs that are entirely off the surface before touching any pixel.
            if (left < canvas.width() && left + w > 0 && top < canvas.height() && top + h > 0) {
                for (int gy = 0; gy < h; gy++) {
                    for (int gx = 0; gx < w; gx++) canvas.blendPixel(left + gx, top + gy, bitmap[gy * w + gx], color);
                }
            }
        }
        pen += advance * scale;
    }
    return pen / canvas.scale();
}

}  // namespace tp
