// Picks up what a mod inside the game reports about the player.
//
// The mod keeps a small block in the game's memory: a 32-byte marker, a word of flags, and a
// counter that moves while the mod is alive. A thread of its
// own finds the block by reading through the game's memory once, then looks at it ten times a
// second. Nothing here knows what the flags mean: packs give them names.
#pragma once
#include <switch.h>

#include <atomic>

class GameStateReader {
public:
    bool start();
    void stop();

    // Whether anybody cares: without a pack that has rules, the game is left alone.
    void setWanted(bool wanted) { m_wanted.store(wanted, std::memory_order_relaxed); }
    // The latest flags; false while there is no live report.
    bool latest(u32& bits) const;

private:
    static void entry(void* self);
    void run();
    bool followBlock(u64 nowMs);
    void scanSlice(u64 nowMs);
    bool tryCandidate(u64 address);
    void forget();
    void publish(bool known, u32 bits);

    Thread m_thread{};
    bool m_started = false;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_wanted{false};
    std::atomic<u64> m_value{0};  // bit 63: known; low 32 bits: the flags

    // Owned by the thread.
    u64 m_pid = 0;
    u64 m_block = 0;        // address of the report, 0 while it is not found
    u32 m_beat = 0;
    u32 m_bits = 0;         // flags from the last time the counter moved
    u64 m_beatMs = 0;       // when the counter last moved
    u64 m_cursor = 0;       // where the search is
    u64 m_regionEnd = 0;    // end of the region being searched
    u64 m_spaceBase = 0;    // the game's address space
    u64 m_spaceEnd = 0;
    u64 m_scanned = 0;      // bytes read in this pass
    u64 m_passStartMs = 0;
    u64 m_restMs = 0;       // no searching before this moment
    bool m_heapOnly = true; // first half of a pass: the game's heap; second half: everything else
    int m_misses = 0;       // passes that found nothing, for spacing them out
    int m_notes = 0;        // log lines written about not getting anywhere
};
