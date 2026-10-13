// The block a game mod keeps in the game's memory to say what the player is doing: a marker
// to find it by, a word of flags, a counter that moves while the mod is alive, and a layout
// version. Each field after the marker is 8 bytes, of which the low 4 are used.
// docs/STATE.md describes it for mod authors.
#pragma once
#include <cstdint>

namespace tp {

constexpr int kStateMarkerSize = 32;
extern const uint8_t kStateMarker[kStateMarkerSize];
constexpr int kStateBlockSize = 56;
constexpr uint32_t kStateVersion = 1;
// Added to the flags by the overlay, never by a mod: the block is there but its counter has
// stopped. A game that freezes its scripts while paused looks like this in its pause menu.
constexpr uint32_t kStateStalled = 0x80000000u;

struct StateBlock {
    uint32_t bits;
    uint32_t beat;
};

// Reads kStateBlockSize bytes. False if they are not a block of the version we understand.
bool readStateBlock(const uint8_t* bytes, StateBlock& out);

// Offset of the first 8-byte step at or after `from` where a marker may begin, -1 if none.
// Only the marker's first 8 bytes are compared, so a block cut off by the end of the data is
// still noticed: the caller reads the whole block from its address to be sure.
int findStateMarker(const uint8_t* data, int size, int from = 0);

}  // namespace tp
