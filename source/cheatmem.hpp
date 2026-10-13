// Reading the running game's memory through Atmosphère's cheat service (dmnt:cht), the same
// way cheat tools do. Used to pick up the state a game mod reports, and for the memory survey.
#pragma once
#include <switch.h>

namespace cheatmem {

struct Extents {
    u64 base;
    u64 size;
};

// What dmnt:cht reports about the process it is attached to.
struct Metadata {
    u64 processId;
    u64 titleId;
    Extents main;
    Extents heap;
    Extents alias;
    Extents addressSpace;
    u8 buildId[0x20];
};

// Connects to the service and attaches it to the running game if nothing has yet.
// `result` (optional) gets the code of the step that failed.
bool open(Result* result = nullptr);
// Detaches again if open() was what attached, and disconnects.
void close();
bool isOpen();

Result metadata(Metadata& out);
Result query(u64 address, MemoryInfo& out);
Result read(u64 address, void* buffer, size_t size);

// One shared read buffer, so that the users of this module do not each carry their own.
// Only one of them works at a time.
constexpr size_t kChunk = 0x8000;
u8* chunk();

}  // namespace cheatmem
