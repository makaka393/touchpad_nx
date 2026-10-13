// Hides the touch screen from the running game while the pad is active, so touches that
// operate the pad are not also delivered to the game as taps.
#pragma once
#include <switch.h>

class TouchGate {
public:
    // block=true: the current application stops receiving touches. Safe to call repeatedly;
    // follows the application if it changes, and re-asserts the block if the system reset it.
    void apply(bool block);
    void restore() { apply(false); }

private:
    u64 m_blockedPid = 0;
    bool m_logged = false;
};
