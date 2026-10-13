// Small string helpers shared by the core.
#pragma once
#include <cstdint>
#include <cstring>

namespace tp {

// Bounded copy that always terminates and never cuts a UTF-8 character in half.
inline void copyText(char* dst, int cap, const char* src) {
    if (cap <= 0) return;
    int n = 0;
    while (src[n] && n < cap - 1) n++;
    if (src[n]) {
        while (n > 0 && (static_cast<unsigned char>(src[n]) & 0xC0) == 0x80) n--;
    }
    std::memcpy(dst, src, static_cast<size_t>(n));
    dst[n] = 0;
}

// Decodes one UTF-8 character and advances p. Malformed bytes come back as U+FFFD, one at a time.
inline uint32_t nextCodepoint(const char*& p) {
    const unsigned char c = static_cast<unsigned char>(*p++);
    if (c < 0x80) return c;
    int extra = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
    if (extra < 0) return 0xFFFD;
    uint32_t cp = c & (0x3F >> extra);
    for (int i = 0; i < extra; i++) {
        const unsigned char next = static_cast<unsigned char>(*p);
        if ((next & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (next & 0x3F);
        p++;
    }
    return cp;
}

}  // namespace tp
