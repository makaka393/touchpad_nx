// Look of the controls: one accent colour, an opacity level, optional coloured face buttons.
#pragma once
#include <cstdint>

namespace tp {

struct Theme {
    uint8_t r = 15, g = 15, b = 15;  // accent colour, 4 bits per channel
    uint8_t opacity = 4;             // kMinOpacity (faint) .. kMaxOpacity (solid)
    bool faceColors = false;         // A/B/X/Y each in their own colour
    bool operator==(const Theme&) const = default;
};

constexpr int kMinOpacity = 1;
constexpr int kMaxOpacity = 5;

struct ColorPreset {
    const char* name;
    uint8_t r, g, b;
};

constexpr ColorPreset kColorPresets[] = {
    {"White", 15, 15, 15}, {"Cyan", 5, 14, 15},    {"Green", 6, 15, 7},
    {"Yellow", 15, 14, 4}, {"Orange", 15, 9, 3},   {"Red", 15, 5, 5},
    {"Pink", 15, 7, 12},   {"Purple", 11, 8, 15},  {"Blue", 6, 9, 15},
};
constexpr int kColorPresetCount = sizeof(kColorPresets) / sizeof(kColorPresets[0]);

// Index of the preset the theme's colour matches, or -1 for a hand-picked colour.
constexpr int presetIndex(const Theme& t) {
    for (int i = 0; i < kColorPresetCount; i++) {
        if (kColorPresets[i].r == t.r && kColorPresets[i].g == t.g && kColorPresets[i].b == t.b) return i;
    }
    return -1;
}

}  // namespace tp
