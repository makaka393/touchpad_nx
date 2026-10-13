#include "canvas.hpp"

#include <cmath>
#include <cstring>

namespace tp {

namespace {

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Signed distance to a rounded box centred on the origin.
inline float sdRoundBox(float px, float py, float hw, float hh, float r) {
    const float qx = std::fabs(px) - (hw - r);
    const float qy = std::fabs(py) - (hh - r);
    const float ox = qx > 0 ? qx : 0;
    const float oy = qy > 0 ? qy : 0;
    const float inside = qx > qy ? qx : qy;
    return std::sqrt(ox * ox + oy * oy) + (inside < 0 ? inside : 0) - r;
}

inline float sdCross(float px, float py, float armLen, float armHalfWidth, float r) {
    const float h = sdRoundBox(px, py, armLen, armHalfWidth, r);
    const float v = sdRoundBox(px, py, armHalfWidth, armLen, r);
    return h < v ? h : v;
}

// Straight-alpha "source over" on a 4-bit-per-channel pixel (r in the low nibble, a in the high).
inline void blend(uint16_t& dst, Color c, float coverage) {
    const float sa = (c.a / 15.0f) * coverage;
    const uint16_t d = dst;
    const int da4 = d >> 12;

    if (da4 == 0) {
        const int a = static_cast<int>(sa * 15.0f + 0.5f);
        if (a > 0) dst = static_cast<uint16_t>(c.r | (c.g << 4) | (c.b << 8) | (a << 12));
        return;
    }

    const float da = da4 / 15.0f;
    const float oa = sa + da * (1.0f - sa);
    const float ws = sa / oa;
    const float wd = 1.0f - ws;
    const int r = static_cast<int>(c.r * ws + (d & 0xF) * wd + 0.5f);
    const int g = static_cast<int>(c.g * ws + ((d >> 4) & 0xF) * wd + 0.5f);
    const int b = static_cast<int>(c.b * ws + ((d >> 8) & 0xF) * wd + 0.5f);
    const int a = static_cast<int>(oa * 15.0f + 0.5f);
    dst = static_cast<uint16_t>(r | (g << 4) | (b << 8) | (a << 12));
}

// The same for a buffer whose colours are already multiplied by their alpha, which is what the
// console's compositor expects: it adds the colour as it is and only dims what is underneath.
inline void blendPremultiplied(uint16_t& dst, Color c, float coverage) {
    const float sa = (c.a / 15.0f) * coverage;
    const uint16_t d = dst;
    const float keep = 1.0f - sa;
    auto mix = [&](int src, int under) {
        const int v = static_cast<int>(src * sa + under * keep + 0.5f);
        return v > 15 ? 15 : v;
    };
    const int a = mix(15, d >> 12);
    if (a == 0) return;
    dst = static_cast<uint16_t>(mix(c.r, d & 0xF) | (mix(c.g, (d >> 4) & 0xF) << 4) | (mix(c.b, (d >> 8) & 0xF) << 8) | (a << 12));
}

}  // namespace

void Canvas::put(uint16_t& dst, Color c, float coverage) const {
    if (m_premultiplied) blendPremultiplied(dst, c, coverage);
    else blend(dst, c, coverage);
}

Canvas::Canvas(uint16_t* pixels, int width, int height, int stridePixels)
    : m_px(pixels), m_w(width), m_h(height), m_stride(stridePixels),
      m_scale(static_cast<float>(width) / kScreenW) {
    resetClip();
}

void Canvas::setClip(const Rect& logical) {
    const int x0 = static_cast<int>(std::floor(logical.x0 * m_scale));
    const int y0 = static_cast<int>(std::floor(logical.y0 * m_scale));
    const int x1 = static_cast<int>(std::ceil(logical.x1 * m_scale));
    const int y1 = static_cast<int>(std::ceil(logical.y1 * m_scale));
    m_cx0 = x0 < 0 ? 0 : x0;
    m_cy0 = y0 < 0 ? 0 : y0;
    m_cx1 = x1 > m_w ? m_w : x1;
    m_cy1 = y1 > m_h ? m_h : y1;
}

void Canvas::resetClip() {
    m_cx0 = 0;
    m_cy0 = 0;
    m_cx1 = m_w;
    m_cy1 = m_h;
}

void Canvas::clear() {
    if (m_cx1 <= m_cx0) return;
    for (int y = m_cy0; y < m_cy1; y++) {
        std::memset(m_px + y * m_stride + m_cx0, 0, static_cast<size_t>(m_cx1 - m_cx0) * sizeof(uint16_t));
    }
}

template <typename Sdf>
void Canvas::shade(float bx0, float by0, float bx1, float by1, Color c, Sdf sdf) {
    if (c.a == 0) return;
    int x0 = static_cast<int>(std::floor(bx0 * m_scale)) - 1;
    int y0 = static_cast<int>(std::floor(by0 * m_scale)) - 1;
    int x1 = static_cast<int>(std::ceil(bx1 * m_scale)) + 1;
    int y1 = static_cast<int>(std::ceil(by1 * m_scale)) + 1;
    if (x0 < m_cx0) x0 = m_cx0;
    if (y0 < m_cy0) y0 = m_cy0;
    if (x1 > m_cx1) x1 = m_cx1;
    if (y1 > m_cy1) y1 = m_cy1;

    const float inv = 1.0f / m_scale;
    for (int y = y0; y < y1; y++) {
        const float ly = (y + 0.5f) * inv;
        uint16_t* row = m_px + y * m_stride;
        for (int x = x0; x < x1; x++) {
            const float lx = (x + 0.5f) * inv;
            // Distance in framebuffer pixels; half a pixel either side of the edge is the AA ramp.
            const float coverage = 0.5f - sdf(lx, ly) * m_scale;
            if (coverage <= 0.0f) continue;
            put(row[x], c, coverage > 1.0f ? 1.0f : coverage);
        }
    }
}

void Canvas::blendPixel(int fx, int fy, uint8_t coverage, Color c) {
    if (fx < m_cx0 || fx >= m_cx1 || fy < m_cy0 || fy >= m_cy1 || coverage == 0) return;
    put(m_px[fy * m_stride + fx], c, coverage / 255.0f);
}

void Canvas::fillCircle(float cx, float cy, float r, Color c) {
    shade(cx - r, cy - r, cx + r, cy + r, c, [=](float x, float y) {
        const float dx = x - cx, dy = y - cy;
        return std::sqrt(dx * dx + dy * dy) - r;
    });
}

void Canvas::strokeCircle(float cx, float cy, float r, float width, Color c) {
    const float hw = width * 0.5f;
    const float o = r + hw;
    shade(cx - o, cy - o, cx + o, cy + o, c, [=](float x, float y) {
        const float dx = x - cx, dy = y - cy;
        return std::fabs(std::sqrt(dx * dx + dy * dy) - r) - hw;
    });
}

void Canvas::fillRoundRect(float cx, float cy, float hw, float hh, float radius, Color c) {
    shade(cx - hw, cy - hh, cx + hw, cy + hh, c,
          [=](float x, float y) { return sdRoundBox(x - cx, y - cy, hw, hh, radius); });
}

void Canvas::strokeRoundRect(float cx, float cy, float hw, float hh, float radius, float width, Color c) {
    const float half = width * 0.5f;
    shade(cx - hw - half, cy - hh - half, cx + hw + half, cy + hh + half, c,
          [=](float x, float y) { return std::fabs(sdRoundBox(x - cx, y - cy, hw, hh, radius)) - half; });
}

void Canvas::fillCross(float cx, float cy, float armLen, float armHalfWidth, float radius, Color c) {
    shade(cx - armLen, cy - armLen, cx + armLen, cy + armLen, c,
          [=](float x, float y) { return sdCross(x - cx, y - cy, armLen, armHalfWidth, radius); });
}

void Canvas::strokeCross(float cx, float cy, float armLen, float armHalfWidth, float radius, float width, Color c) {
    const float half = width * 0.5f;
    const float o = armLen + half;
    shade(cx - o, cy - o, cx + o, cy + o, c, [=](float x, float y) {
        return std::fabs(sdCross(x - cx, y - cy, armLen, armHalfWidth, radius)) - half;
    });
}

void Canvas::line(float x0, float y0, float x1, float y1, float width, Color c) {
    const float half = width * 0.5f;
    const float ex = x1 - x0, ey = y1 - y0;
    const float len2 = ex * ex + ey * ey;
    const float minX = (x0 < x1 ? x0 : x1) - half, maxX = (x0 > x1 ? x0 : x1) + half;
    const float minY = (y0 < y1 ? y0 : y1) - half, maxY = (y0 > y1 ? y0 : y1) + half;
    shade(minX, minY, maxX, maxY, c, [=](float x, float y) {
        const float px = x - x0, py = y - y0;
        const float t = len2 > 0 ? clampf((px * ex + py * ey) / len2, 0.0f, 1.0f) : 0.0f;
        const float dx = px - ex * t, dy = py - ey * t;
        return std::sqrt(dx * dx + dy * dy) - half;
    });
}

void Canvas::drawMask(const uint8_t* mask, int size, float cx, float cy, float half, Color c) {
    const float x0 = (cx - half) * m_scale;
    const float y0 = (cy - half) * m_scale;
    const float span = 2.0f * half * m_scale;  // framebuffer pixels across
    if (span < 1.0f) return;
    const float step = size / span;            // mask pixels per framebuffer pixel
    auto texel = [&](int tx, int ty) { return (mask[(ty * size + tx) >> 1] >> (((ty * size + tx) & 1) * 4)) & 15; };

    const int fx0 = static_cast<int>(std::floor(x0)), fx1 = static_cast<int>(std::ceil(x0 + span));
    const int fy0 = static_cast<int>(std::floor(y0)), fy1 = static_cast<int>(std::ceil(y0 + span));
    for (int fy = fy0; fy < fy1; fy++) {
        // The part of the mask this framebuffer row covers; averaging over it keeps a
        // shrunken pictogram smooth.
        const float v0 = std::fmax(0.0f, (fy - y0) * step), v1 = std::fmin(static_cast<float>(size), (fy + 1 - y0) * step);
        if (v1 <= v0) continue;
        for (int fx = fx0; fx < fx1; fx++) {
            const float u0 = std::fmax(0.0f, (fx - x0) * step), u1 = std::fmin(static_cast<float>(size), (fx + 1 - x0) * step);
            if (u1 <= u0) continue;
            float sum = 0;
            for (int ty = static_cast<int>(v0); ty < size && ty < v1; ty++) {
                const float wy = std::fmin(v1, ty + 1.0f) - std::fmax(v0, static_cast<float>(ty));
                for (int tx = static_cast<int>(u0); tx < size && tx < u1; tx++) {
                    const float wx = std::fmin(u1, tx + 1.0f) - std::fmax(u0, static_cast<float>(tx));
                    sum += texel(tx, ty) * wx * wy;
                }
            }
            const int coverage = static_cast<int>(sum / (step * step) * (255.0f / 15.0f) + 0.5f);
            if (coverage > 0) blendPixel(fx, fy, static_cast<uint8_t>(coverage > 255 ? 255 : coverage), c);
        }
    }
}

}  // namespace tp
