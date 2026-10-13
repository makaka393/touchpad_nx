#include "pack.hpp"

#include "config.hpp"
#include "text.hpp"

#include <cstring>

namespace tp {

namespace {

// Game packs that ship inside the overlay: the same text files as in packs/, so they double
// as examples of the format. A file with the same name on the card replaces the built-in one.
const unsigned char kMk8d[] = {
#embed "../packs/mk8d.ini"
    , 0};
const unsigned char kLm3[] = {
#embed "../packs/lm3.ini"
    , 0};
const unsigned char kBurnout[] = {
#embed "../packs/burnout.ini"
    , 0};

struct Embedded {
    const char* id;
    const unsigned char* text;
};
constexpr Embedded kEmbedded[] = {{"mk8d", kMk8d}, {"lm3", kLm3}, {"burnout", kBurnout}};

const char* textOf(const Embedded& e) { return reinterpret_cast<const char*>(e.text); }

constexpr uint32_t kStickClicks = BtnStickL | BtnStickR;

// A one-layout pack cut from the full pad.
void addGeneric(PackStore& store, const char* id, const char* name, uint32_t shownButtons, bool stickL, bool stickR, bool dpad) {
    if (store.count == kMaxPacks) return;
    Pack& p = store.items[store.count++];
    copyText(p.id, sizeof(p.id), id);
    copyText(p.name, sizeof(p.name), name);
    p.author[0] = 0;
    p.gameCount = 0;
    p.layoutCount = 1;
    p.start = 0;
    p.flagCount = 0;

    NamedLayout& only = p.layouts[0];
    only.when.terms = 0;
    only.holds = false;
    copyText(only.id, sizeof(only.id), "main");
    copyText(only.name, sizeof(only.name), name);
    Layout& l = only.layout;
    l = defaultLayout();
    for (int i = 0; i < l.buttonCount; i++) l.buttons[i].visible = (shownButtons & l.buttons[i].bit) != 0;
    l.stickL.visible = stickL;
    l.stickR.visible = stickR;
    l.dpad.visible = dpad;
}

}  // namespace

int Pack::findLayout(const char* layoutId) const {
    for (int i = 0; i < layoutCount; i++) {
        if (std::strcmp(layouts[i].id, layoutId) == 0) return i;
    }
    return -1;
}

uint32_t Pack::flagMask(const char* name) const {
    for (int i = 0; i < flagCount; i++) {
        if (std::strcmp(flags[i].name, name) == 0) return flags[i].mask;
    }
    return 0;
}

int Pack::layoutFor(uint32_t state) const {
    for (int i = layoutCount - 1; i >= 0; i--) {
        if (layouts[i].when.matches(state)) return i;
    }
    return -1;
}

bool Pack::followsGameState() const {
    for (int i = 0; i < layoutCount; i++) {
        if (layouts[i].when.terms > 0) return true;
    }
    return false;
}

bool Pack::madeFor(uint64_t titleId) const {
    for (int i = 0; i < gameCount; i++) {
        if (games[i] == titleId) return true;
    }
    return false;
}

int PackStore::find(const char* id) const {
    for (int i = 0; i < count; i++) {
        if (std::strcmp(items[i].id, id) == 0) return i;
    }
    return -1;
}

int PackStore::findForGame(uint64_t titleId) const {
    if (!titleId) return -1;
    // Later packs win: files from the card are loaded after the built-in ones.
    for (int i = count - 1; i >= 0; i--) {
        if (items[i].madeFor(titleId)) return i;
    }
    return -1;
}

int PackStore::put(const Pack& pack) {
    int i = find(pack.id);
    if (i < 0) {
        if (count == kMaxPacks) return -1;
        i = count++;
    }
    items[i] = pack;
    return i;
}

void addBuiltinPacks(PackStore& store) {
    const uint32_t face = BtnA | BtnB | BtnX | BtnY;
    const uint32_t shoulders = BtnL | BtnR | BtnZL | BtnZR;
    const uint32_t system = BtnPlus | BtnMinus | BtnHome;
    const uint32_t all = face | shoulders | system | BtnCapture | kStickClicks;

    addGeneric(store, "full", "All buttons", all, true, true, true);
    addGeneric(store, "platformer", "Platformer", face | BtnL | BtnR | system, true, false, false);
    addGeneric(store, "dpad", "D-pad", face | BtnL | BtnR | system, false, false, true);
    addGeneric(store, "minimal", "Minimal", BtnA | BtnB | BtnPlus | BtnHome, true, false, false);

    for (const Embedded& e : kEmbedded) {
        if (store.count == kMaxPacks) return;
        Pack& p = store.items[store.count];
        copyText(p.id, sizeof(p.id), e.id);
        if (parsePack(textOf(e), p)) store.count++;
    }
}

const char* builtinPackText(int index, const char** id) {
    const int n = static_cast<int>(sizeof(kEmbedded) / sizeof(kEmbedded[0]));
    if (index < 0 || index >= n) return nullptr;
    if (id) *id = kEmbedded[index].id;
    return textOf(kEmbedded[index]);
}

}  // namespace tp
