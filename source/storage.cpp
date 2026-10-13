#include "storage.hpp"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

#include "../core/text.hpp"
#include "log.hpp"

namespace storage {

namespace {

constexpr const char* kDir = "sdmc:/config/touchpad";
constexpr const char* kPacksDir = "sdmc:/config/touchpad/packs";
constexpr const char* kLegacyDir = "sdmc:/config/touchpad/profiles";  // layouts saved by 0.2
constexpr const char* kSettingsPath = "sdmc:/config/touchpad/settings.ini";
constexpr const char* kUltrahandConfig = "sdmc:/config/ultrahand/config.ini";

// One shared scratch buffer. Used by main before the input thread exists, and only by the
// input thread afterwards, so never by two threads at once.
char g_text[24 * 1024];

// A pack is read here first, so a broken file never damages the pack it would replace.
// Same threading rule as the text buffer.
tp::Pack g_pack;

bool readFile(const char* path) {
    g_text[0] = 0;
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    const size_t n = std::fread(g_text, 1, sizeof(g_text) - 1, f);
    std::fclose(f);
    g_text[n] = 0;
    return n > 0;
}

bool writeFile(const char* path, int length) {
    if (length <= 0) return false;
    mkdir(kDir, 0777);
    mkdir(kPacksDir, 0777);
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const bool ok = std::fwrite(g_text, 1, static_cast<size_t>(length), f) == static_cast<size_t>(length);
    std::fclose(f);
    return ok;
}

// "mk8d.ini" -> "mk8d". Only plain lowercase names are accepted as pack ids.
bool packIdFromFile(const char* fileName, char* id, size_t cap) {
    const size_t len = std::strlen(fileName);
    if (len < 5 || len - 4 >= cap || std::strcmp(fileName + len - 4, ".ini") != 0) return false;
    for (size_t i = 0; i < len - 4; i++) {
        const char c = fileName[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
        id[i] = c;
    }
    id[len - 4] = 0;
    return true;
}

}  // namespace

void load(tp::App& app) {
    if (readFile(kSettingsPath)) tp::parseSettings(g_text, app.settings());

    tp::PackStore& store = app.packs();

    // Layouts the player arranged under 0.2 carry over to the generic pack of the same name.
    // They are only read: once such a pack is saved again it lands in packs/, which wins below.
    int carried = 0;
    if (DIR* dir = opendir(kLegacyDir)) {
        while (dirent* entry = readdir(dir)) {
            char id[sizeof(tp::Pack::id)];
            if (!packIdFromFile(entry->d_name, id, sizeof(id))) continue;
            const int at = store.find(id);
            if (at < 0 || store.items[at].gameCount > 0 || store.items[at].layoutCount != 1) continue;

            char path[320];  // a directory entry name may be 255 bytes
            std::snprintf(path, sizeof(path), "%s/%s", kLegacyDir, entry->d_name);
            if (!readFile(path)) continue;
            tp::parseLegacyProfile(g_text, store.items[at].layouts[0].layout);
            carried++;
        }
        closedir(dir);
    }

    int fromCard = 0;
    if (DIR* dir = opendir(kPacksDir)) {
        while (dirent* entry = readdir(dir)) {
            char id[sizeof(tp::Pack::id)];
            if (!packIdFromFile(entry->d_name, id, sizeof(id))) continue;

            char path[320];
            std::snprintf(path, sizeof(path), "%s/%s", kPacksDir, entry->d_name);
            if (!readFile(path)) continue;

            tp::copyText(g_pack.id, sizeof(g_pack.id), id);
            tp::PackReport report;
            const bool usable = tp::parsePack(g_text, g_pack, &report);
            if (report.badLines > 0) {
                logx::line("storage: %s: %d line(s) not understood, first is line %d: %s", entry->d_name, report.badLines,
                           report.firstBadLine, report.firstBad);
            }
            if (!usable) {
                logx::line("storage: %s has no layout, skipped", entry->d_name);
                continue;
            }
            if (store.put(g_pack) >= 0) fromCard++;
            else logx::line("storage: no room for %s (%d packs at most)", entry->d_name, tp::kMaxPacks);
        }
        closedir(dir);
    }
    const bool legacy = app.settings().legacy;
    app.adoptLegacySettings();
    logx::line("storage: %d packs (%d from the card, %d layouts carried over from 0.2), %d games remembered%s", store.count,
               fromCard, carried, app.settings().gameCount, legacy ? ", settings converted from 0.2" : "");
}

void saveSettings(const tp::Settings& settings) {
    if (!writeFile(kSettingsPath, tp::writeSettings(settings, g_text, sizeof(g_text)))) {
        logx::line("storage: could not write settings.ini");
    }
}

void savePack(const tp::Pack& pack) {
    char path[128];
    std::snprintf(path, sizeof(path), "%s/%s.ini", kPacksDir, pack.id);
    const bool ok = writeFile(path, tp::writePack(pack, g_text, sizeof(g_text)));
    logx::line("storage: pack %s %s", pack.id, ok ? "saved" : "could NOT be saved");
}

u32 ultrahandCombo() {
    char combo[64] = "";
    if (readFile(kUltrahandConfig)) tp::findIniValue(g_text, "ultrahand", "key_combo", combo, sizeof(combo));
    u32 mask = tp::parseCombo(combo);
    if (!mask) mask = tp::parseCombo("ZL+ZR+DDOWN");  // Ultrahand's default
    return mask;
}

}  // namespace storage
