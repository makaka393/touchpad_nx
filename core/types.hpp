// Platform-independent types shared by the touch mapper, renderer and the Switch glue.
#pragma once
#include <cstdint>

namespace tp {

// Logical screen space: the handheld panel, same units HID reports touches in.
constexpr int kScreenW = 1280;
constexpr int kScreenH = 720;

// Bit values match HidNpadButton / HiddbgNpadButton so the state can be handed to HDLS as-is.
enum Button : uint32_t {
    BtnA       = 1u << 0,
    BtnB       = 1u << 1,
    BtnX       = 1u << 2,
    BtnY       = 1u << 3,
    BtnStickL  = 1u << 4,
    BtnStickR  = 1u << 5,
    BtnL       = 1u << 6,
    BtnR       = 1u << 7,
    BtnZL      = 1u << 8,
    BtnZR      = 1u << 9,
    BtnPlus    = 1u << 10,
    BtnMinus   = 1u << 11,
    BtnLeft    = 1u << 12,
    BtnUp      = 1u << 13,
    BtnRight   = 1u << 14,
    BtnDown    = 1u << 15,
    BtnHome    = 1u << 18,
    BtnCapture = 1u << 19,
};

constexpr int kStickMax = 32767;

struct TouchPoint {
    uint32_t id;
    int x;
    int y;
};

struct PadState {
    uint32_t buttons = 0;
    int16_t lx = 0, ly = 0;  // +x right, +y up
    int16_t rx = 0, ry = 0;
    bool operator==(const PadState&) const = default;
};

// Everything the renderer needs to know to draw the current frame.
struct Visual {
    PadState pad;
    bool paused = false;
    uint8_t menuHold = 0;  // 0 = not held, 1..kMenuHoldSteps = hold-to-exit progress
    uint32_t touched = 0;  // buttons under a finger, one bit per index in the layout
    // Sticks a finger is holding, and where that finger put their centre (floating sticks).
    bool grabL = false, grabR = false;
    int16_t lox = 0, loy = 0, rox = 0, roy = 0;
    bool operator==(const Visual&) const = default;
};

constexpr int kMenuHoldSteps = 4;

}  // namespace tp
