#include "tinytest.hpp"
#include "../core/stateblock.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace tp;

namespace {

// A block the way a game mod lays it out: the marker and three 8-byte script variables.
std::vector<uint8_t> block(uint32_t bits, uint32_t beat, uint32_t version = kStateVersion) {
    std::vector<uint8_t> b(kStateBlockSize, 0);
    std::memcpy(b.data(), kStateMarker, kStateMarkerSize);
    std::memcpy(b.data() + 32, &bits, 4);
    std::memcpy(b.data() + 40, &beat, 4);
    std::memcpy(b.data() + 48, &version, 4);
    return b;
}

}  // namespace

TEST(state_block_gives_its_flags_and_counter) {
    StateBlock s{};
    CHECK(readStateBlock(block(0x40100001, 77).data(), s));
    CHECK_EQ(s.bits, 0x40100001);
    CHECK_EQ(s.beat, 77);
}

TEST(state_block_ignores_the_unused_upper_halves) {
    // A script variable is 8 bytes and the game only promises the low 4.
    std::vector<uint8_t> b = block(5, 6);
    std::memset(b.data() + 36, 0xEE, 4);
    std::memset(b.data() + 44, 0xEE, 4);
    StateBlock s{};
    CHECK(readStateBlock(b.data(), s));
    CHECK_EQ(s.bits, 5);
    CHECK_EQ(s.beat, 6);
}

TEST(state_block_with_a_damaged_marker_or_another_version_is_not_one) {
    StateBlock s{};
    std::vector<uint8_t> bad = block(1, 2);
    bad[31] ^= 1;
    CHECK(!readStateBlock(bad.data(), s));
    CHECK(!readStateBlock(block(1, 2, kStateVersion + 1).data(), s));
    CHECK(!readStateBlock(std::vector<uint8_t>(kStateBlockSize, 0).data(), s));
}

TEST(marker_is_found_on_eight_byte_steps_only) {
    std::vector<uint8_t> memory(4096, 0x11);
    const std::vector<uint8_t> b = block(1, 2);
    std::memcpy(memory.data() + 1024, b.data(), b.size());
    std::memcpy(memory.data() + 2051, b.data(), b.size());  // not where a script variable can be
    CHECK_EQ(findStateMarker(memory.data(), 4096), 1024);
    CHECK_EQ(findStateMarker(memory.data(), 4096, 1024), 1024);
    CHECK_EQ(findStateMarker(memory.data(), 4096, 1025), -1);
    CHECK_EQ(findStateMarker(memory.data(), 1024), -1);
}

TEST(marker_cut_off_by_the_end_of_the_data_is_still_noticed) {
    std::vector<uint8_t> memory(64, 0);
    std::memcpy(memory.data() + 56, kStateMarker, 8);
    CHECK_EQ(findStateMarker(memory.data(), 64), 56);
    CHECK_EQ(findStateMarker(memory.data(), 63), -1);
    CHECK_EQ(findStateMarker(memory.data(), 0), -1);
}
