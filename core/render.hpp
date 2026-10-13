// Draws a Scene: the pad, and on top of it the settings menu or the layout editor.
// During play only the controls whose state changed are repainted.
#pragma once
#include "app.hpp"
#include "canvas.hpp"
#include "font.hpp"

namespace tp {

class Renderer {
public:
    // `font` may be null or not loaded; text is then left out.
    explicit Renderer(Canvas& canvas, const Font* font = nullptr);

    // Returns true if any pixel was repainted (i.e. the frame needs presenting).
    bool draw(const Scene& scene);

private:
    static constexpr int kMaxElements = Layout::kMaxButtons + 4;

    // Where a stick is drawn right now: a floating one sits under the thumb holding it.
    struct StickPose {
        int16_t x, y;
        bool shown;
        bool operator==(const StickPose&) const = default;
    };

    struct Element {
        enum Kind : uint8_t { ButtonEl, StickLEl, StickREl, DpadEl, MenuEl } kind;
        uint8_t index;   // button index for ButtonEl
        Rect bounds;
        uint32_t key;    // last drawn state
        StickPose pose = {0, 0, false};  // sticks only
    };

    struct Style {
        Color fill, shadow, line, glyph;
        float outline;  // ring thickness
        Color fillDown, lineDown, glyphDown;
        Color baseFill, baseLine, knobIdle, knobActive;
    };

    void rebuildElements();
    Style styleFor(uint8_t r, uint8_t g, uint8_t b, bool ghost) const;
    Style styleForButton(const ButtonDef& b) const;
    bool elementShown(const Element& e) const;
    bool elementSelected(const Element& e) const;

    const StickDef& stickOf(const Element& e) const;
    StickPose poseFor(const Element& e, const Visual& v) const;
    bool buttonLit(int index, const Visual& v) const;
    uint32_t keyFor(const Element& e, const Visual& v) const;
    uint32_t knobKey(const StickDef& s, int16_t ax, int16_t ay) const;
    Rect dirtyFor(const Element& e, uint32_t oldKey, uint32_t newKey) const;
    Rect knobBounds(const Element& e, uint32_t key) const;

    void repaint(const Rect& area, const Visual& v);
    void drawElement(const Element& e, const Visual& v);
    void drawButton(const ButtonDef& b, bool pressed, const Style& st);
    void drawStick(const Element& e, int16_t ax, int16_t ay, const Style& st);
    float fitText(const char* utf8, float maxSize, float maxWidth) const;
    void drawDpad(uint32_t buttons, const Style& st);
    void drawMenuButton(const Visual& v);
    void drawMenu();
    void drawEditor();
    void drawSelection(const Element& e);
    void text(float x, float baseline, float size, const char* utf8, Color c);
    void textCentered(float cx, float baseline, float size, const char* utf8, Color c);
    void caption(float cx, float baseline, float size, const char* utf8, Color c);

    Canvas& m_canvas;
    const Font* m_font;
    Scene m_scene;             // what is currently on the canvas (layout, theme, menu state)
    Element m_elements[kMaxElements];
    int m_elementCount = 0;
    bool m_first = true;
};

}  // namespace tp
