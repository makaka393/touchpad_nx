#include "layout.hpp"

namespace tp {

// Arranged like a pair of Joy-Cons: left stick above the d-pad, face buttons above the right stick,
// shoulders in the top corners, system buttons along the top edge.
const Layout& defaultLayout() {
    static const Layout layout = {
        {
            {BtnZL, Shape::RoundRect, 92, 62, 78, 46, Glyph::ZL},
            {BtnL, Shape::RoundRect, 278, 62, 78, 46, Glyph::L},
            {BtnR, Shape::RoundRect, 1002, 62, 78, 46, Glyph::R},
            {BtnZR, Shape::RoundRect, 1188, 62, 78, 46, Glyph::ZR},

            {BtnA, Shape::Circle, 1202, 300, 52, 52, Glyph::A},
            {BtnB, Shape::Circle, 1110, 392, 52, 52, Glyph::B},
            {BtnX, Shape::Circle, 1110, 208, 52, 52, Glyph::X},
            {BtnY, Shape::Circle, 1018, 300, 52, 52, Glyph::Y},

            {BtnMinus, Shape::Circle, 456, 46, 30, 30, Glyph::Minus},
            {BtnCapture, Shape::Circle, 548, 46, 30, 30, Glyph::Capture},
            {BtnHome, Shape::Circle, 732, 46, 30, 30, Glyph::Home},
            {BtnPlus, Shape::Circle, 824, 46, 30, 30, Glyph::Plus},

            {BtnStickL, Shape::Circle, 80, 560, 40, 40, Glyph::StickClick},
            {BtnStickR, Shape::Circle, 1200, 560, 40, 40, Glyph::StickClick},
        },
        14,
        {170, 300, 105, 140, 85},
        {950, 565, 105, 140, 85},
        {330, 565, 105, 38, 125, 18},
        {640, 46, 30},
        14,
    };
    return layout;
}

void setStickSize(StickDef& stick, int baseRadius) {
    stick.baseRadius = static_cast<int16_t>(baseRadius);
    stick.zoneRadius = static_cast<int16_t>(baseRadius + 35);
    stick.throwRadius = static_cast<int16_t>(baseRadius * 81 / 100);
}

void setDpadSize(DpadDef& dpad, int armLen) {
    dpad.armLen = static_cast<int16_t>(armLen);
    dpad.armHalfWidth = static_cast<int16_t>(armLen * 362 / 1000);
    dpad.zoneRadius = static_cast<int16_t>(armLen + 20);
}

}  // namespace tp
