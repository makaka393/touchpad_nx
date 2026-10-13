// Text formats kept on the SD card: settings, packs, and the bits of Ultrahand's config we read.
#pragma once
#include "pack.hpp"
#include "theme.hpp"

namespace tp {

struct GameBinding {
    uint64_t titleId;
    char packId[16];
};

constexpr int kMaxGames = 48;

struct Settings {
    Theme theme;
    char defaultPack[16] = "full";
    GameBinding games[kMaxGames];
    int gameCount = 0;
    // The text was written by 0.2, which knew profiles but no game packs.
    bool legacy = false;

    const char* packFor(uint64_t titleId) const;      // nullptr when nothing was chosen for the game
    void bind(uint64_t titleId, const char* packId);  // remembers the pack chosen for a game
    void unbind(uint64_t titleId);                    // the game goes back to its automatic pack
};

void parseSettings(const char* text, Settings& out);
int writeSettings(const Settings& settings, char* buf, int cap);  // returns the text length

// What was wrong with a pack file, for its author.
struct PackReport {
    int badLines = 0;
    int firstBadLine = 0;  // 1-based
    char firstBad[64] = "";
};

// Reads a pack file into `out`, replacing everything but its id. Lines that make no sense are
// skipped and counted in `report`. Returns false when the text holds no layout at all.
bool parsePack(const char* text, Pack& out, PackReport* report = nullptr);
int writePack(const Pack& pack, char* buf, int cap);  // returns the text length, 0 if it does not fit

// Reads a 0.2 profile ("A=x,y,half-width,half-height,shown", "stick_l=x,y,size,shown") onto a
// layout that already holds the standard pad; controls the text does not mention keep their place.
void parseLegacyProfile(const char* text, Layout& out);

// Name used for a pictogram in pack files ("run", "car", ...); "" for Glyph::None.
const char* glyphName(Glyph glyph);

// "ZL+ZR+DDOWN" -> button mask. 0 if the string is empty or names a key we cannot produce.
uint32_t parseCombo(const char* combo);

// Looks up key in [section] of an ini text. Returns false (and leaves out empty) if absent.
bool findIniValue(const char* text, const char* section, const char* key, char* out, int cap);

}  // namespace tp
