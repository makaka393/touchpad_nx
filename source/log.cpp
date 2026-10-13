#include "log.hpp"

#include <switch.h>

#include <cstdarg>
#include <cstdio>
#include <sys/stat.h>

namespace logx {

namespace {
constexpr const char* kDir = "sdmc:/config/touchpad";
constexpr const char* kPath = "sdmc:/config/touchpad/log.txt";

// Horizon refuses to open a file for writing twice, so two threads logging at the same
// moment would silently lose a line. One writer at a time.
Mutex g_lock = 0;
}

void begin() {
    mutexLock(&g_lock);
    mkdir("sdmc:/config", 0777);
    mkdir(kDir, 0777);
    if (FILE* f = std::fopen(kPath, "w")) std::fclose(f);
    mutexUnlock(&g_lock);
}

// Opened and closed per line so nothing is lost if the overlay dies right after.
void line(const char* fmt, ...) {
    mutexLock(&g_lock);
    if (FILE* f = std::fopen(kPath, "a")) {
        va_list args;
        va_start(args, fmt);
        std::vfprintf(f, fmt, args);
        va_end(args);
        std::fputc('\n', f);
        std::fclose(f);
    }
    mutexUnlock(&g_lock);
}

}  // namespace logx
