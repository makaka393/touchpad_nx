// Pictograms for game actions (run, jump, enter a car, ...): ready-made icons shipped as
// coverage masks, so they look the same at any size and in any colour.
#pragma once
#include "canvas.hpp"
#include "layout.hpp"

namespace tp {

// Draws the pictogram centred on (cx, cy) in a box about 1.8 * size across.
// Returns false for glyphs that are not pictograms (None and the controller legends).
bool drawIcon(Canvas& canvas, Glyph glyph, float cx, float cy, float size, Color color);

}  // namespace tp
