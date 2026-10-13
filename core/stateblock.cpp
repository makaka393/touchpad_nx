#include "stateblock.hpp"

#include <cstring>

namespace tp {

// "TouchPadGameState block v1 " and five bytes no text would have.
const uint8_t kStateMarker[kStateMarkerSize] = {'T', 'o', 'u', 'c', 'h', 'P', 'a', 'd', 'G', 'a', 'm', 'e', 'S', 't', 'a', 't',
                                                'e', ' ', 'b', 'l', 'o', 'c', 'k', ' ', 'v', '1', ' ', 0xA5, 0x3C, 0xE1, 0x94, 0x2B};

namespace {

uint32_t low32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 | static_cast<uint32_t>(p[2]) << 16 |
           static_cast<uint32_t>(p[3]) << 24;
}

}  // namespace

bool readStateBlock(const uint8_t* bytes, StateBlock& out) {
    if (std::memcmp(bytes, kStateMarker, kStateMarkerSize) != 0) return false;
    if (low32(bytes + 48) != kStateVersion) return false;
    out.bits = low32(bytes + 32);
    out.beat = low32(bytes + 40);
    return true;
}

int findStateMarker(const uint8_t* data, int size, int from) {
    if (from < 0) from = 0;
    from = (from + 7) & ~7;
    for (int at = from; at + 8 <= size; at += 8) {
        if (std::memcmp(data + at, kStateMarker, 8) == 0) return at;
    }
    return -1;
}

}  // namespace tp
