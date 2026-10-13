#include "config.hpp"

#include "text.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace tp {

namespace {

struct KeyName {
    uint32_t bit;
    const char* name;
};

// Same spellings Ultrahand uses for key combos, plus HOME and CAPTURE.
constexpr KeyName kKeys[] = {
    {BtnA, "A"},         {BtnB, "B"},         {BtnX, "X"},           {BtnY, "Y"},
    {BtnL, "L"},         {BtnR, "R"},         {BtnZL, "ZL"},         {BtnZR, "ZR"},
    {BtnPlus, "PLUS"},   {BtnMinus, "MINUS"}, {BtnHome, "HOME"},     {BtnCapture, "CAPTURE"},
    {BtnStickL, "LS"},   {BtnStickR, "RS"},   {BtnLeft, "DLEFT"},    {BtnUp, "DUP"},
    {BtnRight, "DRIGHT"}, {BtnDown, "DDOWN"},
};

constexpr int kMinStick = 60, kMaxStick = 150;
constexpr int kMinDpad = 60, kMaxDpad = 150;

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

bool equalsNoCase(const char* a, const char* b) {
    for (; *a && *b; a++, b++) {
        const char ca = (*a >= 'a' && *a <= 'z') ? *a - 32 : *a;
        const char cb = (*b >= 'a' && *b <= 'z') ? *b - 32 : *b;
        if (ca != cb) return false;
    }
    return *a == *b;
}

uint32_t keyBit(const char* name) {
    for (const KeyName& k : kKeys) {
        if (equalsNoCase(k.name, name)) return k.bit;
    }
    return 0;
}

// Copies [begin, end) with surrounding blanks removed. Returns false if it does not fit.
bool trimCopy(const char* begin, const char* end, char* out, int cap) {
    while (begin < end && (*begin == ' ' || *begin == '\t')) begin++;
    while (end > begin && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) end--;
    const int len = static_cast<int>(end - begin);
    if (len >= cap) {
        out[0] = 0;
        return false;
    }
    std::memcpy(out, begin, len);
    out[len] = 0;
    return true;
}

// Calls fn(section, key, value, lineNumber) for every "key = value" line of an ini text, and
// bad(lineNumber, lineText) for every line that is neither that, a section, a comment nor blank.
template <typename Fn, typename Bad>
void iniWalk(const char* text, Fn fn, Bad bad) {
    char section[32] = "";
    char key[48];
    char value[224];
    int number = 0;
    const char* line = text;
    while (*line) {
        const char* end = line;
        while (*end && *end != '\n') end++;
        number++;

        const char* p = line;
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r')) p++;
        if (p < end && *p == '[') {
            const char* close = p;
            while (close < end && *close != ']') close++;
            if (close < end) trimCopy(p + 1, close, section, sizeof(section));
            else bad(number, p);
        } else if (p < end && *p != ';' && *p != '#') {
            const char* eq = p;
            while (eq < end && *eq != '=') eq++;
            if (eq < end && trimCopy(p, eq, key, sizeof(key)) && trimCopy(eq + 1, end, value, sizeof(value)) && key[0]) {
                fn(section, key, value, number);
            } else {
                bad(number, p);
            }
        }
        line = *end ? end + 1 : end;
    }
}

template <typename Fn>
void iniForEach(const char* text, Fn fn) {
    iniWalk(text, [&](const char* section, const char* key, const char* value, int) { fn(section, key, value); },
            [](int, const char*) {});
}

// Reads up to `max` comma separated integers. Returns how many were read; -1 on a malformed list.
int parseInts(const char* s, int* out, int max) {
    int n = 0;
    while (*s) {
        char* end = nullptr;
        const long v = std::strtol(s, &end, 10);
        if (end == s || n == max) return -1;
        out[n++] = static_cast<int>(v);
        s = end;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == ',') s++;
        else if (*s) return -1;
    }
    return n;
}

int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Appends formatted text; once the buffer is exhausted `len` is pinned to -1.
struct Writer {
    char* buf;
    int cap;
    int len = 0;

    __attribute__((format(printf, 2, 3))) void add(const char* fmt, ...) {
        if (len < 0) return;
        va_list args;
        va_start(args, fmt);
        const int n = std::vsnprintf(buf + len, cap - len, fmt, args);
        va_end(args);
        len = (n < 0 || n >= cap - len) ? -1 : len + n;
    }

    int finish() {
        if (len < 0) {
            if (cap > 0) buf[0] = 0;
            return 0;
        }
        return len;
    }
};

}  // namespace

const char* Settings::packFor(uint64_t titleId) const {
    for (int i = 0; i < gameCount; i++) {
        if (games[i].titleId == titleId) return games[i].packId;
    }
    return nullptr;
}

void Settings::bind(uint64_t titleId, const char* packId) {
    int at = -1;
    for (int i = 0; i < gameCount; i++) {
        if (games[i].titleId == titleId) at = i;
    }
    if (at < 0) {
        if (gameCount == kMaxGames) {
            // Oldest entry makes room: the list is kept in the order games were first seen.
            std::memmove(&games[0], &games[1], sizeof(GameBinding) * (kMaxGames - 1));
            gameCount--;
        }
        at = gameCount++;
        games[at].titleId = titleId;
    }
    copyText(games[at].packId, sizeof(games[at].packId), packId);
}

void Settings::unbind(uint64_t titleId) {
    for (int i = 0; i < gameCount; i++) {
        if (games[i].titleId != titleId) continue;
        std::memmove(&games[i], &games[i + 1], sizeof(GameBinding) * (gameCount - i - 1));
        gameCount--;
        return;
    }
}

void parseSettings(const char* text, Settings& out) {
    bool sawPack = false, sawProfile = false;
    iniForEach(text, [&](const char* section, const char* key, const char* value) {
        if (std::strcmp(section, "general") == 0) {
            sawPack |= std::strcmp(key, "pack") == 0;
            sawProfile |= std::strcmp(key, "profile") == 0;
        }
        if (!value[0]) return;
        if (std::strcmp(section, "theme") == 0) {
            if (std::strcmp(key, "color") == 0) {
                if (value[0] == '#' && std::strlen(value) == 4) {
                    const int r = hexNibble(value[1]), g = hexNibble(value[2]), b = hexNibble(value[3]);
                    if (r >= 0 && g >= 0 && b >= 0) {
                        out.theme.r = static_cast<uint8_t>(r);
                        out.theme.g = static_cast<uint8_t>(g);
                        out.theme.b = static_cast<uint8_t>(b);
                    }
                }
            } else if (std::strcmp(key, "opacity") == 0) {
                out.theme.opacity = static_cast<uint8_t>(clampi(std::atoi(value), kMinOpacity, kMaxOpacity));
            } else if (std::strcmp(key, "face_colors") == 0) {
                out.theme.faceColors = std::atoi(value) != 0;
            }
        } else if (std::strcmp(section, "general") == 0) {
            // "profile" is what 0.2 called it.
            if (std::strcmp(key, "pack") == 0 || std::strcmp(key, "profile") == 0) copyText(out.defaultPack, sizeof(out.defaultPack), value);
        } else if (std::strcmp(section, "games") == 0) {
            char* end = nullptr;
            const unsigned long long id = std::strtoull(key, &end, 16);
            if (end != key && *end == 0 && id != 0) out.bind(id, value);
        }
    });
    out.legacy = sawProfile && !sawPack;
}

int writeSettings(const Settings& s, char* buf, int cap) {
    Writer w{buf, cap};
    w.add("; TouchPad settings. color is #RGB, one hex digit per channel; opacity is %d..%d.\n", kMinOpacity, kMaxOpacity);
    w.add("[general]\npack=%s\n\n", s.defaultPack);
    w.add("[theme]\ncolor=#%X%X%X\nopacity=%d\nface_colors=%d\n\n", s.theme.r, s.theme.g, s.theme.b, s.theme.opacity,
          s.theme.faceColors ? 1 : 0);
    w.add("; pack chosen for each game (title id = pack file name)\n[games]\n");
    for (int i = 0; i < s.gameCount; i++) {
        w.add("%016llX=%s\n", static_cast<unsigned long long>(s.games[i].titleId), s.games[i].packId);
    }
    return w.finish();
}

namespace {

struct GlyphName {
    Glyph glyph;
    const char* name;
};

constexpr GlyphName kGlyphs[] = {
    {Glyph::A, "a"},           {Glyph::B, "b"},           {Glyph::X, "x"},         {Glyph::Y, "y"},
    {Glyph::L, "l"},           {Glyph::R, "r"},           {Glyph::ZL, "zl"},       {Glyph::ZR, "zr"},
    {Glyph::Plus, "plus"},     {Glyph::Minus, "minus"},   {Glyph::Home, "home"},   {Glyph::Capture, "capture"},
    {Glyph::StickClick, "stick"},
#define ICON(id, name) {Glyph::id, name},
#include "icon_list.inc"
#undef ICON
};

// Legend for a button whose pack line names neither an icon nor a label.
Glyph glyphForKeys(uint32_t bits) {
    switch (bits) {
    case BtnA: return Glyph::A;
    case BtnB: return Glyph::B;
    case BtnX: return Glyph::X;
    case BtnY: return Glyph::Y;
    case BtnL: return Glyph::L;
    case BtnR: return Glyph::R;
    case BtnZL: return Glyph::ZL;
    case BtnZR: return Glyph::ZR;
    case BtnPlus: return Glyph::Plus;
    case BtnMinus: return Glyph::Minus;
    case BtnHome: return Glyph::Home;
    case BtnCapture: return Glyph::Capture;
    case BtnStickL:
    case BtnStickR: return Glyph::StickClick;
    case BtnUp: return Glyph::Up;
    case BtnDown: return Glyph::Down;
    case BtnLeft: return Glyph::Left;
    case BtnRight: return Glyph::Right;
    }
    return Glyph::None;
}

constexpr int kMaxFields = 12;
constexpr int kMinCircle = 20, kMaxCircle = 120;
constexpr int kMinRectW = 20, kMaxRectW = 220, kMinRectH = 20, kMaxRectH = 120;

// Splits "a | b | c" in place into trimmed fields. Returns how many there are, -1 if too many.
int splitFields(char* s, char** fields, int max) {
    int n = 0;
    char* p = s;
    while (true) {
        char* end = p;
        while (*end && *end != '|') end++;
        const bool last = *end == 0;
        while (*p == ' ' || *p == '\t') p++;
        char* tail = end;
        while (tail > p && (tail[-1] == ' ' || tail[-1] == '\t')) tail--;
        *tail = 0;
        if (n == max) return -1;
        fields[n++] = p;
        if (last) return n;
        p = end + 1;
    }
}

// "name=value" -> value when the field is that attribute, nullptr otherwise.
const char* attr(const char* field, const char* name) {
    const size_t len = std::strlen(name);
    if (std::strncmp(field, name, len) != 0) return nullptr;
    const char* p = field + len;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '=') return nullptr;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

bool parseColor(const char* v, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (v[0] != '#' || std::strlen(v) != 4) return false;
    const int rr = hexNibble(v[1]), gg = hexNibble(v[2]), bb = hexNibble(v[3]);
    if (rr < 0 || gg < 0 || bb < 0) return false;
    r = static_cast<uint8_t>(rr);
    g = static_cast<uint8_t>(gg);
    b = static_cast<uint8_t>(bb);
    return true;
}

bool validId(const char* s, int cap) {
    int n = 0;
    for (; s[n]; n++) {
        const char c = s[n];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return n > 0 && n < cap;
}

// A layout with nothing on it: the pack file adds what it wants.
void clearLayout(Layout& l) {
    l = defaultLayout();
    l.buttonCount = 0;
    l.stickL.visible = false;
    l.stickR.visible = false;
    l.dpad.visible = false;
}

bool readButton(const Pack& pack, Layout& l, char** f, int n) {
    if (n < 2 || l.buttonCount == Layout::kMaxButtons) return false;

    ButtonDef b{};
    if (f[0][0] && !equalsNoCase(f[0], "NONE") && std::strcmp(f[0], "-") != 0) {
        b.bit = parseCombo(f[0]);
        if (!b.bit) return false;
    }

    int v[4];
    const int ints = parseInts(f[1], v, 4);
    if (ints == 3) {
        b.shape = Shape::Circle;
        b.rx = b.ry = static_cast<int16_t>(clampi(v[2], kMinCircle, kMaxCircle));
    } else if (ints == 4) {
        b.shape = Shape::RoundRect;
        b.rx = static_cast<int16_t>(clampi(v[2], kMinRectW, kMaxRectW));
        b.ry = static_cast<int16_t>(clampi(v[3], kMinRectH, kMaxRectH));
    } else {
        return false;
    }
    b.cx = static_cast<int16_t>(clampi(v[0], b.rx, kScreenW - b.rx));
    b.cy = static_cast<int16_t>(clampi(v[1], b.ry, kScreenH - b.ry));

    bool named = false;
    for (int i = 2; i < n; i++) {
        if (const char* a = attr(f[i], "icon")) {
            b.glyph = Glyph::None;
            for (const GlyphName& g : kGlyphs) {
                if (equalsNoCase(g.name, a)) b.glyph = g.glyph;
            }
            if (b.glyph == Glyph::None) return false;
            named = true;
        } else if (const char* a = attr(f[i], "label")) {
            copyText(b.label, sizeof(b.label), a);
            named = true;
        } else if (const char* a = attr(f[i], "mode")) {
            if (equalsNoCase(a, "hold")) b.press = Press::Hold;
            else if (equalsNoCase(a, "toggle")) b.press = Press::Toggle;
            else if (equalsNoCase(a, "turbo")) b.press = Press::Turbo;
            else return false;
        } else if (const char* a = attr(f[i], "color")) {
            if (!parseColor(a, b.r, b.g, b.b)) return false;
            b.tinted = true;
        } else if (const char* a = attr(f[i], "drag")) {
            if (equalsNoCase(a, "ls")) b.drag = Drag::StickL;
            else if (equalsNoCase(a, "rs")) b.drag = Drag::StickR;
            else return false;
        } else if (const char* a = attr(f[i], "goto")) {
            if (equalsNoCase(a, "back")) {
                b.gotoLayout = kGotoBack;
            } else {
                const int target = pack.findLayout(a);
                if (target < 0) return false;
                b.gotoLayout = static_cast<int8_t>(target);
            }
        } else if (const char* a = attr(f[i], "hidden")) {
            b.visible = std::atoi(a) == 0;
        } else {
            return false;
        }
    }
    if (!named) {
        b.glyph = glyphForKeys(b.bit);
        if (b.glyph == Glyph::None && b.bit) copyText(b.label, sizeof(b.label), f[0]);
    }

    l.buttons[l.buttonCount++] = b;
    return true;
}

bool readStick(StickDef& s, char** f, int n) {
    int v[4];
    if (n < 1 || parseInts(f[0], v, 3) != 3) return false;
    StickDef next = s;
    setStickSize(next, clampi(v[2], kMinStick, kMaxStick));
    next.cx = static_cast<int16_t>(clampi(v[0], next.baseRadius, kScreenW - next.baseRadius));
    next.cy = static_cast<int16_t>(clampi(v[1], next.baseRadius, kScreenH - next.baseRadius));
    next.visible = true;
    next.floating = false;
    next.hint = true;
    for (int i = 1; i < n; i++) {
        if (const char* a = attr(f[i], "zone")) {
            if (parseInts(a, v, 4) != 4) return false;
            next.zx0 = static_cast<int16_t>(clampi(v[0], 0, kScreenW));
            next.zy0 = static_cast<int16_t>(clampi(v[1], 0, kScreenH));
            next.zx1 = static_cast<int16_t>(clampi(v[2], 0, kScreenW));
            next.zy1 = static_cast<int16_t>(clampi(v[3], 0, kScreenH));
            if (next.zx1 <= next.zx0 || next.zy1 <= next.zy0) return false;
            next.floating = true;
        } else if (const char* a = attr(f[i], "hint")) {
            next.hint = std::atoi(a) != 0;
        } else if (const char* a = attr(f[i], "hidden")) {
            next.visible = std::atoi(a) == 0;
        } else {
            return false;
        }
    }
    s = next;
    return true;
}

bool readDpad(DpadDef& d, char** f, int n) {
    int v[3];
    if (n < 1 || parseInts(f[0], v, 3) != 3) return false;
    bool visible = true;
    for (int i = 1; i < n; i++) {
        const char* a = attr(f[i], "hidden");
        if (!a) return false;
        visible = std::atoi(a) == 0;
    }
    setDpadSize(d, clampi(v[2], kMinDpad, kMaxDpad));
    d.cx = static_cast<int16_t>(clampi(v[0], d.armLen, kScreenW - d.armLen));
    d.cy = static_cast<int16_t>(clampi(v[1], d.armLen, kScreenH - d.armLen));
    d.visible = visible;
    return true;
}

bool readGames(const char* value, Pack& out) {
    out.gameCount = 0;
    const char* p = value;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (!*p) break;
        char* end = nullptr;
        const unsigned long long id = std::strtoull(p, &end, 16);
        if (end == p || id == 0 || out.gameCount == kMaxPackGames) return false;
        out.games[out.gameCount++] = id;
        p = end;
    }
    return true;
}

// "vehicle & !plane | boat": terms joined by |, each a set of flags that must be on or (with !) off.
bool readWhen(const Pack& pack, const char* text, WhenRule& out) {
    WhenRule rule{};
    const char* p = text;
    while (true) {
        if (rule.terms == WhenRule::kMaxTerms) return false;
        uint32_t mask = 0, value = 0;
        while (true) {
            while (*p == ' ' || *p == '\t') p++;
            const bool off = *p == '!';
            if (off) p++;
            while (*p == ' ' || *p == '\t') p++;
            char name[sizeof(StateFlag::name)];
            int n = 0;
            while ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-') {
                if (n == static_cast<int>(sizeof(name)) - 1) return false;
                name[n++] = *p++;
            }
            name[n] = 0;
            const uint32_t bit = pack.flagMask(name);
            if (!bit) return false;
            mask |= bit;
            if (!off) value |= bit;
            while (*p == ' ' || *p == '\t') p++;
            if (*p != '&') break;
            p++;
        }
        rule.mask[rule.terms] = mask;
        rule.value[rule.terms] = value;
        rule.terms++;
        if (*p == 0) break;
        if (*p != '|') return false;
        p++;
    }
    out = rule;
    return true;
}

void writeWhen(Writer& w, const Pack& p, const WhenRule& rule) {
    if (rule.terms == 0) return;
    w.add("when=");
    for (int t = 0; t < rule.terms; t++) {
        if (t) w.add(" | ");
        bool first = true;
        for (int i = 0; i < p.flagCount; i++) {
            const uint32_t bit = p.flags[i].mask;
            if ((rule.mask[t] & bit) != bit) continue;
            w.add("%s%s%s", first ? "" : " & ", (rule.value[t] & bit) == bit ? "" : "!", p.flags[i].name);
            first = false;
        }
    }
    w.add("\n");
}

// "ZL+ZR" for a button mask; "NONE" for an empty one.
void comboText(uint32_t bits, char* out, int cap) {
    Writer w{out, cap};
    bool first = true;
    for (const KeyName& k : kKeys) {
        if (!(bits & k.bit)) continue;
        w.add("%s%s", first ? "" : "+", k.name);
        first = false;
    }
    if (first) w.add("NONE");
    w.finish();
}

void writeStick(Writer& w, const char* key, const StickDef& s) {
    w.add("%s=%d,%d,%d", key, s.cx, s.cy, s.baseRadius);
    if (s.floating) w.add(" | zone=%d,%d,%d,%d", s.zx0, s.zy0, s.zx1, s.zy1);
    if (s.floating && !s.hint) w.add(" | hint=0");
    if (!s.visible) w.add(" | hidden=1");
    w.add("\n");
}

}  // namespace

void parseLegacyProfile(const char* text, Layout& l) {
    iniForEach(text, [&](const char*, const char* key, const char* value) {
        int v[5];
        const int n = parseInts(value, v, 5);

        const bool isStickL = equalsNoCase(key, "stick_l");
        const bool isStickR = equalsNoCase(key, "stick_r");
        if (isStickL || isStickR || equalsNoCase(key, "dpad")) {
            if (n != 4) return;
            if (isStickL || isStickR) {
                StickDef& s = isStickL ? l.stickL : l.stickR;
                setStickSize(s, clampi(v[2], kMinStick, kMaxStick));
                s.cx = static_cast<int16_t>(clampi(v[0], s.baseRadius, kScreenW - s.baseRadius));
                s.cy = static_cast<int16_t>(clampi(v[1], s.baseRadius, kScreenH - s.baseRadius));
                s.visible = v[3] != 0;
            } else {
                DpadDef& d = l.dpad;
                setDpadSize(d, clampi(v[2], kMinDpad, kMaxDpad));
                d.cx = static_cast<int16_t>(clampi(v[0], d.armLen, kScreenW - d.armLen));
                d.cy = static_cast<int16_t>(clampi(v[1], d.armLen, kScreenH - d.armLen));
                d.visible = v[3] != 0;
            }
            return;
        }

        const uint32_t bit = keyBit(key);
        if (!bit || n != 5) return;
        for (int i = 0; i < l.buttonCount; i++) {
            ButtonDef& b = l.buttons[i];
            if (b.bit != bit) continue;
            const bool round = b.shape == Shape::Circle;
            b.rx = static_cast<int16_t>(clampi(v[2], round ? kMinCircle : kMinRectW, round ? kMaxCircle : kMaxRectW));
            b.ry = static_cast<int16_t>(round ? b.rx : clampi(v[3], kMinRectH, kMaxRectH));
            b.cx = static_cast<int16_t>(clampi(v[0], b.rx, kScreenW - b.rx));
            b.cy = static_cast<int16_t>(clampi(v[1], b.ry, kScreenH - b.ry));
            b.visible = v[4] != 0;
        }
    });
}

const char* glyphName(Glyph glyph) {
    for (const GlyphName& g : kGlyphs) {
        if (g.glyph == glyph) return g.name;
    }
    return "";
}

bool parsePack(const char* text, Pack& out, PackReport* report) {
    out.name[0] = 0;
    out.author[0] = 0;
    out.gameCount = 0;
    out.layoutCount = 0;
    out.start = 0;
    out.flagCount = 0;

    auto bad = [&](int line, const char* what) {
        if (!report) return;
        if (report->badLines++ == 0) {
            report->firstBadLine = line;
            int n = 0;
            while (what[n] && what[n] != '\n' && what[n] != '\r' && n < static_cast<int>(sizeof(report->firstBad)) - 1) n++;
            char cut[sizeof(report->firstBad)];
            std::memcpy(cut, what, static_cast<size_t>(n));
            cut[n] = 0;
            copyText(report->firstBad, sizeof(report->firstBad), cut);
        }
    };

    // First pass: every section but [pack] is a layout. They are all known before any control
    // is read, so a button can point at a layout defined further down.
    iniWalk(
        text,
        [&](const char* section, const char* key, const char* value, int) {
            if (std::strcmp(section, "pack") == 0 || out.findLayout(section) >= 0) return;
            if (std::strcmp(section, "state") == 0) {
                // Flags are known before any layout rule is read, wherever [state] sits in the file.
                char* end = nullptr;
                const unsigned long mask = std::strtoul(value, &end, 0);
                if (end == value || *end || mask == 0 || mask > 0xFFFFFFFFul) return;
                if (!validId(key, sizeof(StateFlag::name)) || out.flagMask(key) || out.flagCount == kMaxStateFlags) return;
                StateFlag& flag = out.flags[out.flagCount++];
                copyText(flag.name, sizeof(flag.name), key);
                flag.mask = static_cast<uint32_t>(mask);
                return;
            }
            if (!validId(section, sizeof(NamedLayout::id)) || out.layoutCount == kMaxLayouts) return;
            NamedLayout& l = out.layouts[out.layoutCount++];
            copyText(l.id, sizeof(l.id), section);
            copyText(l.name, sizeof(l.name), section);
            clearLayout(l.layout);
            l.when.terms = 0;
            l.holds = false;
        },
        [](int, const char*) {});

    iniWalk(
        text,
        [&](const char* section, const char* key, const char* value, int line) {
            char copy[224];
            copyText(copy, sizeof(copy), value);
            bool ok = true;

            if (std::strcmp(section, "pack") == 0) {
                if (std::strcmp(key, "name") == 0) copyText(out.name, sizeof(out.name), value);
                else if (std::strcmp(key, "author") == 0) copyText(out.author, sizeof(out.author), value);
                else if (std::strcmp(key, "games") == 0) ok = readGames(value, out);
                else if (std::strcmp(key, "start") == 0) {
                    const int start = out.findLayout(value);
                    ok = start >= 0;
                    if (ok) out.start = start;
                } else ok = false;
            } else if (std::strcmp(section, "state") == 0) {
                ok = out.flagMask(key) != 0;  // read in the first pass; a line that did not make it is a bad one
            } else {
                const int at = out.findLayout(section);
                if (at < 0) {
                    ok = false;
                } else {
                    NamedLayout& named = out.layouts[at];
                    Layout& l = named.layout;
                    char* f[kMaxFields];
                    const int n = splitFields(copy, f, kMaxFields);
                    if (std::strcmp(key, "name") == 0) {
                        copyText(named.name, sizeof(named.name), value);
                    } else if (std::strcmp(key, "when") == 0) {
                        ok = readWhen(out, value, named.when);
                    } else if (std::strcmp(key, "hold") == 0) {
                        named.holds = std::atoi(value) != 0;
                    } else if (std::strcmp(key, "touch") == 0) {
                        if (equalsNoCase(value, "game")) l.touchToGame = true;
                        else if (equalsNoCase(value, "pad")) l.touchToGame = false;
                        else ok = false;
                    } else if (std::strcmp(key, "menu") == 0) {
                        int v[3];
                        const int ints = parseInts(value, v, 3);
                        ok = ints == 2 || ints == 3;
                        if (ok) {
                            l.menu.r = static_cast<int16_t>(ints == 3 ? clampi(v[2], 20, 60) : l.menu.r);
                            l.menu.cx = static_cast<int16_t>(clampi(v[0], l.menu.r, kScreenW - l.menu.r));
                            l.menu.cy = static_cast<int16_t>(clampi(v[1], l.menu.r, kScreenH - l.menu.r));
                        }
                    } else if (n < 0) {
                        ok = false;
                    } else if (std::strcmp(key, "btn") == 0) {
                        ok = readButton(out, l, f, n);
                    } else if (std::strcmp(key, "stick_l") == 0) {
                        ok = readStick(l.stickL, f, n);
                    } else if (std::strcmp(key, "stick_r") == 0) {
                        ok = readStick(l.stickR, f, n);
                    } else if (std::strcmp(key, "dpad") == 0) {
                        ok = readDpad(l.dpad, f, n);
                    } else {
                        ok = false;
                    }
                }
            }

            if (!ok) {
                char what[48 + 224 + 2];  // key + '=' + value at their longest
                std::snprintf(what, sizeof(what), "%s=%s", key, value);
                bad(line, what);
            }
        },
        bad);

    if (!out.name[0]) copyText(out.name, sizeof(out.name), out.id);
    return out.layoutCount > 0;
}

int writePack(const Pack& p, char* buf, int cap) {
    Writer w{buf, cap};
    w.add("; TouchPad pack. Screen is 1280x720, x to the right, y down.\n");
    w.add("; btn = KEYS | x,y,radius  or  x,y,half-width,half-height | icon= label= mode= color= drag= goto=\n");
    w.add("[pack]\nname=%s\n", p.name);
    if (p.author[0]) w.add("author=%s\n", p.author);
    if (p.gameCount > 0) {
        w.add("games=");
        for (int i = 0; i < p.gameCount; i++) w.add("%s%016llX", i ? "," : "", static_cast<unsigned long long>(p.games[i]));
        w.add("\n");
    }
    if (p.layoutCount > 0) w.add("start=%s\n", p.layouts[p.start].id);
    if (p.flagCount > 0) {
        w.add("\n; bits of the state word the game mod reports\n[state]\n");
        for (int i = 0; i < p.flagCount; i++) w.add("%s=0x%X\n", p.flags[i].name, static_cast<unsigned>(p.flags[i].mask));
    }

    for (int at = 0; at < p.layoutCount; at++) {
        const NamedLayout& named = p.layouts[at];
        const Layout& l = named.layout;
        w.add("\n[%s]\nname=%s\n", named.id, named.name);
        writeWhen(w, p, named.when);
        if (named.holds) w.add("hold=1\n");
        if (l.touchToGame) w.add("touch=game\n");
        w.add("menu=%d,%d,%d\n", l.menu.cx, l.menu.cy, l.menu.r);
        writeStick(w, "stick_l", l.stickL);
        writeStick(w, "stick_r", l.stickR);
        w.add("dpad=%d,%d,%d%s\n", l.dpad.cx, l.dpad.cy, l.dpad.armLen, l.dpad.visible ? "" : " | hidden=1");
        for (int i = 0; i < l.buttonCount; i++) {
            const ButtonDef& b = l.buttons[i];
            char keys[96];
            comboText(b.bit, keys, sizeof(keys));
            if (b.shape == Shape::Circle) w.add("btn=%s | %d,%d,%d", keys, b.cx, b.cy, b.rx);
            else w.add("btn=%s | %d,%d,%d,%d", keys, b.cx, b.cy, b.rx, b.ry);
            if (b.glyph != Glyph::None) w.add(" | icon=%s", glyphName(b.glyph));
            if (b.label[0]) w.add(" | label=%s", b.label);
            if (b.press == Press::Toggle) w.add(" | mode=toggle");
            if (b.press == Press::Turbo) w.add(" | mode=turbo");
            if (b.tinted) w.add(" | color=#%X%X%X", b.r & 15, b.g & 15, b.b & 15);
            if (b.drag == Drag::StickL) w.add(" | drag=ls");
            if (b.drag == Drag::StickR) w.add(" | drag=rs");
            if (b.gotoLayout == kGotoBack) w.add(" | goto=back");
            else if (b.gotoLayout >= 0 && b.gotoLayout < p.layoutCount) w.add(" | goto=%s", p.layouts[b.gotoLayout].id);
            if (!b.visible) w.add(" | hidden=1");
            w.add("\n");
        }
    }
    return w.finish();
}

uint32_t parseCombo(const char* combo) {
    uint32_t mask = 0;
    const char* p = combo;
    while (true) {
        const char* end = p;
        while (*end && *end != '+') end++;
        char token[16];
        if (!trimCopy(p, end, token, sizeof(token)) || !token[0]) return 0;
        const uint32_t bit = keyBit(token);
        if (!bit) return 0;
        mask |= bit;
        if (!*end) return mask;
        p = end + 1;
    }
}

bool findIniValue(const char* text, const char* section, const char* key, char* out, int cap) {
    bool found = false;
    if (cap > 0) out[0] = 0;
    iniForEach(text, [&](const char* s, const char* k, const char* v) {
        if (found || std::strcmp(s, section) != 0 || std::strcmp(k, key) != 0) return;
        copyText(out, cap, v);
        found = true;
    });
    return found;
}

}  // namespace tp
