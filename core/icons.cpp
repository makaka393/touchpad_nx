#include "icons.hpp"

#include <cstddef>

namespace tp {

namespace {

// Font Awesome Free solid icons (CC BY 4.0, see assets/fontawesome/LICENSE.txt), rendered by
// tools/make_icons.py: kIconCount masks of kMaskSize x kMaskSize pixels, 4 bits each.
constexpr int kMaskSize = 56;
constexpr int kMaskBytes = kMaskSize * kMaskSize / 2;

const unsigned char kMasks[] = {
#embed "icon_data.bin"
};
static_assert(sizeof(kMasks) == static_cast<size_t>(kIconCount) * kMaskBytes, "icon_data.bin does not match icon_list.inc");

}  // namespace

bool drawIcon(Canvas& c, Glyph glyph, float cx, float cy, float size, Color col) {
    const int index = static_cast<int>(glyph) - kFirstIcon;
    if (index < 0 || index >= kIconCount) return false;
    c.drawMask(kMasks + index * kMaskBytes, kMaskSize, cx, cy, size * 0.95f, col);
    return true;
}

}  // namespace tp
