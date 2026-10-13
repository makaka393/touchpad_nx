// A pack is a set of controls made for one game (or a generic one for any game): one or more
// named layouts and the rules for moving between them. Packs live in text files, so new ones
// can be dropped onto the SD card.
#pragma once
#include "layout.hpp"

namespace tp {

// One named bit of the game's state word (what a game mod reports about the player:
// in a vehicle, in the pause menu, ...).
struct StateFlag {
    char name[12];
    uint32_t mask;
};

// When a layout applies by itself: any of up to kMaxTerms terms, each a set of flags that
// must be on (value) among the ones it looks at (mask). No terms: only by hand.
struct WhenRule {
    static constexpr int kMaxTerms = 4;
    uint32_t mask[kMaxTerms];
    uint32_t value[kMaxTerms];
    int terms;

    bool matches(uint32_t state) const {
        for (int i = 0; i < terms; i++) {
            if ((state & mask[i]) == value[i]) return true;
        }
        return false;
    }
};

struct NamedLayout {
    char id[16];    // section name in the pack file, ASCII
    char name[32];  // shown in the menu, UTF-8
    Layout layout;
    WhenRule when;
    // Opened by a button, it stays until a button leaves it, whatever the game reports
    // meanwhile (a menu the player asked for). Without this a button cannot open a layout
    // that has a rule while the game is reporting.
    bool holds;
};

constexpr int kMaxLayouts = 6;
constexpr int kMaxPackGames = 8;
constexpr int kMaxStateFlags = 24;

struct Pack {
    char id[16];      // file name stem, ASCII
    char name[32];    // shown in the menu, UTF-8
    char author[32];
    uint64_t games[kMaxPackGames];  // title ids the pack is made for
    int gameCount;
    NamedLayout layouts[kMaxLayouts];
    int layoutCount;
    int start;  // layout shown when the pack is selected
    StateFlag flags[kMaxStateFlags];  // names the pack gives to bits of the game's state
    int flagCount;

    int findLayout(const char* layoutId) const;  // index or -1
    bool madeFor(uint64_t titleId) const;
    uint32_t flagMask(const char* name) const;   // 0 when the pack has no such flag
    // The layout the game's state calls for: the last one in the pack whose rule matches,
    // so a pack lists the general case first and the special ones after it. -1 when none does.
    int layoutFor(uint32_t state) const;
    bool followsGameState() const;  // any layout has a rule
};

constexpr int kMaxPacks = 16;

struct PackStore {
    Pack items[kMaxPacks];
    int count = 0;

    int find(const char* id) const;         // index or -1
    int findForGame(uint64_t titleId) const;  // first pack made for the game, or -1
    // Replaces the pack with the same id, otherwise appends. Returns its index, -1 when full.
    int put(const Pack& pack);
};

// Generic pads for any game (all buttons, platformer, d-pad, minimal) followed by the game
// packs that ship inside the overlay.
void addBuiltinPacks(PackStore& store);

// Text of a pack that ships inside the overlay, as it would be on the card; nullptr past the end.
const char* builtinPackText(int index, const char** id);

}  // namespace tp
