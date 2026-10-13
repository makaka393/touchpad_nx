// A one-off look at how the running game's memory can be read through Atmosphère's cheat
// service (dmnt:cht), written to the log. It is the groundwork for switching layouts by what
// is really happening in the game; nothing in the overlay depends on it yet.
#pragma once
#include <switch.h>

namespace gameprobe {

// Does nothing unless sdmc:/config/touchpad/probe.txt exists and a game is running.
// Takes a second or two: call it when the pad is no longer in use.
void report();

}  // namespace gameprobe
