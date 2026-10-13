// Tiny append-only log on the SD card; the only way to see what happened on real hardware.
#pragma once

namespace logx {

void begin();                                             // truncates the log
void line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

}  // namespace logx
