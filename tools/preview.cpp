// Host-side preview: renders the overlay exactly as the console would (896x504 RGBA4444),
// scales it up to the screen and composites it over a stand-in game picture. Writes a binary PPM.
//   preview <out.ppm> <play|pressed|hidden|menu|editor|green>
//   preview <out.ppm> pack <file.ini> [layout] [x,y ...]
// The second form checks a pack file and draws one of its layouts (the starting one unless
// named), with fingers at the given points. TOUCHPAD_FONT names the font for captions;
// TOUCHPAD_SCENE=day swaps the dusk stand-in picture for a bright one (the hard case for
// contrast).
#include "../core/app.hpp"
#include "../core/render.hpp"
#include "../core/text.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace tp;

namespace {

App g_app;
uint64_t g_now = 1000;

void feed(std::initializer_list<TouchPoint> touches, uint64_t ms = 8) {
    g_now += ms;
    g_app.update(touches.begin(), static_cast<int>(touches.size()), g_now);
}
void tap(int x, int y) {
    feed({{0, x, y}});
    feed({{0, x, y}}, 40);
    feed({});
}
void tap(const Rect& r) { tap((r.x0 + r.x1) / 2, (r.y0 + r.y1) / 2); }

// Loads a pack file into the app and shows it. Returns false if it cannot be used at all.
bool showPack(int argc, char** argv) {
    if (argc < 4) return false;
    std::vector<char> text;
    if (FILE* f = std::fopen(argv[3], "rb")) {
        std::fseek(f, 0, SEEK_END);
        text.resize(static_cast<size_t>(std::ftell(f)) + 1, 0);
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(text.data(), 1, text.size() - 1, f) != text.size() - 1) text.assign(1, 0);
        std::fclose(f);
    } else {
        std::fprintf(stderr, "cannot read %s\n", argv[3]);
        return false;
    }

    static Pack pack;
    std::string id = argv[3];
    id = id.substr(id.find_last_of('/') + 1);
    id = id.substr(0, id.find_last_of('.'));
    copyText(pack.id, sizeof(pack.id), id.c_str());

    PackReport report;
    const bool usable = parsePack(text.data(), pack, &report);
    if (report.badLines > 0) {
        std::fprintf(stderr, "%s: %d line(s) not understood, first is line %d: %s\n", argv[3], report.badLines,
                     report.firstBadLine, report.firstBad);
    }
    if (!usable) {
        std::fprintf(stderr, "%s: no layout in the file\n", argv[3]);
        return false;
    }

    int layout = -1;
    int next = 4;
    if (argc > next && !std::strchr(argv[next], ',')) {
        layout = pack.findLayout(argv[next]);
        if (layout < 0) {
            std::fprintf(stderr, "%s: no layout [%s]\n", argv[3], argv[next]);
            return false;
        }
        next++;
    }
    g_app.usePack(g_app.packs().put(pack), layout);

    std::vector<TouchPoint> fingers;
    for (; next < argc; next++) {
        int x = 0, y = 0;
        if (std::sscanf(argv[next], "%d,%d", &x, &y) == 2) fingers.push_back({static_cast<uint32_t>(fingers.size() + 1), x, y});
    }
    if (!fingers.empty()) {
        g_now += 8;
        g_app.update(fingers.data(), static_cast<int>(fingers.size()), g_now);
    }

    std::printf("%s: \"%s\", layouts:", pack.id, pack.name);
    for (int i = 0; i < pack.layoutCount; i++) std::printf(" %s%s", pack.layouts[i].id, i == pack.start ? "*" : "");
    std::printf("\n");
    return report.badLines == 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: preview <out.ppm> <play|pressed|hidden|menu|editor|green>\n"
                             "       preview <out.ppm> pack <file.ini> [layout] [x,y ...]\n");
        return 2;
    }
    const char* what = argv[2];
    const char* fontPath = std::getenv("TOUCHPAD_FONT");
    if (!fontPath) fontPath = "/usr/share/fonts/TTF/DejaVuSans.ttf";

    std::vector<unsigned char> fontData;
    if (FILE* f = std::fopen(fontPath, "rb")) {
        std::fseek(f, 0, SEEK_END);
        fontData.resize(static_cast<size_t>(std::ftell(f)));
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(fontData.data(), 1, fontData.size(), f) != fontData.size()) fontData.clear();
        std::fclose(f);
    }
    Font font;
    if (!fontData.empty()) font.load(fontData.data());

    const int fw = 896, fh = 504;  // Layer::kSharpWidth x kSharpHeight
    std::vector<uint16_t> fb(fw * fh, 0);
    Canvas canvas(fb.data(), fw, fh, fw);
    canvas.setPremultiplied(true);  // as on the console
    Renderer renderer(canvas, &font);

    g_app.setGame(0);
    const TouchPoint menuButton{0, 640, 46};
    bool clean = true;
    if (!std::strcmp(what, "pack")) {
        clean = showPack(argc, argv);
    } else if (!std::strcmp(what, "pressed")) {
        feed({{1, 170, 300}, {2, 950, 565}});
        feed({{1, 230, 240}, {2, 870, 565}, {3, 1202, 300}, {4, 1110, 208}, {5, 92, 62}, {6, 1002, 62}, {7, 420, 500}});
    } else if (!std::strcmp(what, "hidden")) {
        tap(menuButton.x, menuButton.y);
        tap(ui::menuRow(RowHide));
    } else if (!std::strcmp(what, "menu")) {
        tap(menuButton.x, menuButton.y);
        const Rect a = ui::menuArrow(RowColor, true);
        feed({{0, (a.x0 + a.x1) / 2, (a.y0 + a.y1) / 2}});  // finger resting on an arrow
    } else if (!std::strcmp(what, "editor")) {
        tap(menuButton.x, menuButton.y);
        tap(ui::menuRow(RowEdit));
        tap(170, 300);                       // select the left stick
        tap(ui::toolButton(ToolBigger));
        tap(1110, 208);                      // select X ...
        tap(ui::toolButton(ToolShow));       // ... and hide it (ghost)
    } else if (!std::strcmp(what, "green")) {
        tap(menuButton.x, menuButton.y);
        tap(ui::menuArrow(RowColor, true));
        tap(ui::menuArrow(RowColor, true));
        tap(ui::menuRow(RowFaceColors));
        tap(menuButton.x, menuButton.y);
        feed({{3, 1110, 392}});
    }

    Scene scene;
    g_app.fillScene(scene);
    renderer.draw(scene);

    const char* sceneName = std::getenv("TOUCHPAD_SCENE");
    const bool brightScene = sceneName && !std::strcmp(sceneName, "day");
    std::vector<unsigned char> out(1280 * 720 * 3);
    for (int y = 0; y < 720; y++) {
        for (int x = 0; x < 1280; x++) {
            float bg[3];
            const float t = y / 719.0f;
            if (brightScene) {
                // Sky-to-grass gradient with bright bands: the hardest picture to read over.
                bg[0] = 70 + 110 * t + ((x / 160) % 2 ? 25 : 0);
                bg[1] = 130 + 60 * (1 - t) + (y > 420 ? 40 : 0);
                bg[2] = 220 - 170 * t;
            } else {
                // A street at dusk: dark sky, a glow on the horizon, asphalt.
                const float horizon = 400.0f;
                if (y < horizon) {
                    const float k = y / horizon;
                    const float glow = k * k * k;
                    bg[0] = 22 + 30 * k + 170 * glow;
                    bg[1] = 30 + 34 * k + 70 * glow;
                    bg[2] = 58 + 40 * k - 10 * glow;
                } else {
                    const float k = (y - horizon) / (719.0f - horizon);
                    bg[0] = 52 - 18 * k;
                    bg[1] = 50 - 16 * k;
                    bg[2] = 56 - 16 * k;
                }
                // A few lit windows on the skyline.
                if (y > 250 && y < horizon && ((x / 90) % 3 == 1) && y > horizon - 60 - ((x / 90) * 37) % 90) {
                    bg[0] = 18; bg[1] = 22; bg[2] = 34;
                    if (((x / 9) % 4 == 1) && ((y / 12) % 3 == 1)) { bg[0] = 230; bg[1] = 200; bg[2] = 120; }
                }
            }

            // Bilinear upscale of the layer, as the display compositor does.
            const float sx = (x + 0.5f) * fw / 1280.0f - 0.5f, sy = (y + 0.5f) * fh / 720.0f - 0.5f;
            int x0 = (int)sx, y0 = (int)sy;
            if (sx < 0) x0 = 0;
            if (sy < 0) y0 = 0;
            const int x1 = x0 + 1 < fw ? x0 + 1 : x0, y1 = y0 + 1 < fh ? y0 + 1 : y0;
            const float fx = sx - x0 < 0 ? 0 : sx - x0, fy = sy - y0 < 0 ? 0 : sy - y0;
            float acc[4] = {0, 0, 0, 0};
            const struct { int px, py; float w; } taps[4] = {
                {x0, y0, (1 - fx) * (1 - fy)}, {x1, y0, fx * (1 - fy)}, {x0, y1, (1 - fx) * fy}, {x1, y1, fx * fy}};
            for (const auto& tp4 : taps) {
                const uint16_t p = fb[tp4.py * fw + tp4.px];
                // Colours in the layer are premultiplied: they are added as they are.
                acc[0] += tp4.w * ((p & 0xF) / 15.0f);
                acc[1] += tp4.w * (((p >> 4) & 0xF) / 15.0f);
                acc[2] += tp4.w * (((p >> 8) & 0xF) / 15.0f);
                acc[3] += tp4.w * ((p >> 12) / 15.0f);
            }
            for (int c = 0; c < 3; c++) {
                const float o = acc[c] * 255.0f + bg[c] * (1.0f - acc[3]);
                out[(y * 1280 + x) * 3 + c] = (unsigned char)(o < 0 ? 0 : o > 255 ? 255 : o);
            }
        }
    }
    FILE* f = std::fopen(argv[1], "wb");
    if (!f) return 1;
    std::fprintf(f, "P6\n1280 720\n255\n");
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    return clean ? 0 : 1;
}
