// Turns the set of fingers currently on the screen into a gamepad state.
#pragma once
#include "layout.hpp"

namespace tp {

struct MapperConfig {
    uint32_t exitHoldMs = 800;     // holding the menu button this long closes the overlay
    float stickDeadzone = 0.06f;   // fraction of the throw radius
    uint32_t turboMs = 90;         // a turbo button is down this long, then up this long
    int dragReach = 90;            // finger travel on a drag button for full stick deflection
};

class Mapper {
public:
    explicit Mapper(const Layout& layout, MapperConfig config = {});

    // touches: every finger currently down. nowMs: any monotonic millisecond clock.
    void update(const TouchPoint* touches, int count, uint64_t nowMs);

    const PadState& pad() const { return m_pad; }
    Visual visual() const;
    bool exitRequested() const { return m_exitRequested; }

    // While paused only the menu button is live; everything else reads as released.
    void setPaused(bool paused);
    bool paused() const { return m_paused; }
    // True once after each short tap on the menu button.
    bool takeMenuTap();
    // The layout a button asked for when it was let go: an index in the pack or kGotoBack.
    // kGotoNone when there is no request; each request is reported once.
    int8_t takeGoto();
    // Forgets all fingers (e.g. after the layout changed); they must lift before acting again.
    void reset();
    // The layout was swapped under the fingers. A thumb on a stick keeps steering if the new
    // layout has that stick; every other finger must lift before it acts again.
    void relayout();

private:
    enum class Role : uint8_t { Free, Anchored, StickL, StickR, Dpad, Menu, Inert };

    struct Finger {
        uint32_t id;
        Role role;
        float originX, originY;  // sticks and drag buttons: current neutral point
        uint64_t downMs;
        bool seen;
        int8_t anchor;           // Anchored: the drag button it landed on
        int x, y;                // where it is now
    };

    static constexpr int kMaxFingers = 16;

    Role classify(int x, int y, int8_t& anchor) const;
    bool hasRole(Role role) const;
    bool inStickZone(const StickDef& def, int x, int y) const;
    void stickFrom(Finger& f, float reach, bool follow, int x, int y, int16_t& outX, int16_t& outY) const;
    uint32_t dpadFrom(int x, int y) const;
    uint32_t buttonsAt(int x, int y) const;  // one bit per button index
    bool onMenu(int x, int y) const;

    const Layout& m_layout;
    MapperConfig m_config;
    Finger m_fingers[kMaxFingers];
    int m_fingerCount = 0;
    PadState m_pad;
    bool m_paused = false;
    bool m_exitRequested = false;
    bool m_menuTapped = false;
    uint8_t m_menuHold = 0;
    uint32_t m_touched = 0;   // buttons under a finger on the last pass, by index
    uint32_t m_latched = 0;   // toggle buttons that are on, by index
    uint64_t m_downMs[Layout::kMaxButtons] = {};
    int8_t m_goto = kGotoNone;
    bool m_grabL = false, m_grabR = false;
    int16_t m_lox = 0, m_loy = 0, m_rox = 0, m_roy = 0;
};

}  // namespace tp
