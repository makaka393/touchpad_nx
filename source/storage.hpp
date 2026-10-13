// Settings and packs on the SD card (sdmc:/config/touchpad).
#pragma once
#include <switch.h>

#include "../core/app.hpp"

namespace storage {

// Fills the app's settings and pack store from the card: every packs/<id>.ini becomes a pack,
// replacing the built-in one of the same name. Missing files are not an error, and a file
// that makes no sense is skipped (the log says which line).
void load(tp::App& app);

void saveSettings(const tp::Settings& settings);
void savePack(const tp::Pack& pack);

// Button mask of Ultrahand's menu combo, read from its config (its default if unreadable).
u32 ultrahandCombo();

}  // namespace storage
