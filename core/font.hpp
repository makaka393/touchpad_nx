// Text on the canvas from a TrueType font (on the console: the system's shared font, which
// covers Cyrillic).
#pragma once
#include <stb_truetype.h>

#include "canvas.hpp"

namespace tp {

class Font {
public:
    // `data` is a whole TrueType file that the caller keeps alive.
    bool load(const unsigned char* data);
    bool ready() const { return m_ready; }

    // Width of UTF-8 text in logical units; `size` is the line height in logical units.
    float width(const Canvas& canvas, float size, const char* utf8) const;
    // Draws with the left end of the baseline at (x, baseline). Returns the width drawn.
    float draw(Canvas& canvas, float x, float baseline, float size, const char* utf8, Color color) const;

private:
    stbtt_fontinfo m_info{};
    bool m_ready = false;
};

}  // namespace tp
