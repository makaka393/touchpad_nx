#include "gamestate.hpp"

#include "../core/stateblock.hpp"
#include "cheatmem.hpp"
#include "log.hpp"

namespace {

constexpr u64 kFollowNs = 100'000'000;      // look at the report ten times a second
constexpr u64 kIdleNs = 500'000'000;        // nothing to do: check twice a second
constexpr u64 kRetryNs = 2'000'000'000;     // the cheat service is not ready: try again later
constexpr u64 kNapNs = 1'000'000;           // between slices of the search
constexpr int kSliceSteps = 8;              // regions looked up or chunks read per slice
constexpr u64 kSettleMs = 20'000;           // leave a game alone this long before looking into it
constexpr u64 kLivenessNs = 250'000'000;    // how long a candidate gets to move its counter
constexpr u64 kStaleMs = 700;               // a counter that stood still this long: the game is paused
// Pause after a pass that found nothing. Reading through the game's memory costs it some speed,
// so a game without the mod is soon left alone for minutes at a time.
constexpr u64 kRescanMs[] = {5'000, 5'000, 15'000, 60'000, 180'000};
constexpr u64 kKnown = 1ull << 63;

u64 nowMs() { return armTicksToNs(armGetSystemTick()) / 1'000'000; }

}  // namespace

bool GameStateReader::start() {
    // Below the input thread: the search only runs while that one sleeps.
    Result rc = threadCreate(&m_thread, entry, this, nullptr, 0x8000, 0x2E, -2);
    if (R_SUCCEEDED(rc)) {
        rc = threadStart(&m_thread);
        if (R_FAILED(rc)) threadClose(&m_thread);
    }
    m_started = R_SUCCEEDED(rc);
    if (!m_started) logx::line("state: reader thread failed: 0x%x", rc);
    return m_started;
}

void GameStateReader::stop() {
    if (!m_started) return;
    m_stop.store(true);
    threadWaitForExit(&m_thread);
    threadClose(&m_thread);
    m_started = false;
}

bool GameStateReader::latest(u32& bits) const {
    const u64 value = m_value.load(std::memory_order_relaxed);
    bits = static_cast<u32>(value);
    return (value & kKnown) != 0;
}

void GameStateReader::publish(bool known, u32 bits) { m_value.store(known ? (kKnown | bits) : 0, std::memory_order_relaxed); }

void GameStateReader::entry(void* self) { static_cast<GameStateReader*>(self)->run(); }

void GameStateReader::forget() {
    m_block = 0;
    publish(false, 0);
    cheatmem::close();
}

// Every pass of this loop sleeps: a sysmodule thread that does not freezes the console.
void GameStateReader::run() {
    while (!m_stop.load(std::memory_order_relaxed)) {
        if (!m_wanted.load(std::memory_order_relaxed)) {
            if (cheatmem::isOpen() || m_block) forget();
            svcSleepThread(kIdleNs);
            continue;
        }

        const u64 now = nowMs();
        if (!cheatmem::isOpen() && m_restMs == 0) m_restMs = now + kSettleMs;  // first look at this game
        if (!m_block && now < m_restMs) {
            svcSleepThread(kIdleNs);
            continue;
        }
        if (!cheatmem::isOpen()) {
            Result rc = 0;
            cheatmem::Metadata meta = {};
            if (!cheatmem::open(&rc) || R_FAILED(rc = cheatmem::metadata(meta))) {
                if (m_notes < 3) {
                    m_notes++;
                    logx::line("state: the cheat service cannot look into the game yet: 0x%x", rc);
                }
                cheatmem::close();
                svcSleepThread(kRetryNs);
                continue;
            }
            if (meta.processId != m_pid) {
                m_notes = 0;
                m_misses = 0;
                logx::line("state: looking for the game's report (process %llu, game %016llX)",
                           static_cast<unsigned long long>(meta.processId), static_cast<unsigned long long>(meta.titleId));
            }
            m_pid = meta.processId;
            m_block = 0;
            m_spaceBase = meta.addressSpace.base;
            m_spaceEnd = meta.addressSpace.base + meta.addressSpace.size;
            m_cursor = m_regionEnd = m_spaceBase;
            m_heapOnly = true;
            m_scanned = 0;
            m_passStartMs = now;
        }

        if (m_block) {
            if (!followBlock(now)) {
                logx::line("state: the report is gone, looking again");
                forget();  // the game may have been restarted: attach afresh
            }
            svcSleepThread(kFollowNs);
        } else {
            scanSlice(now);
            svcSleepThread(kNapNs);
        }
    }
    forget();
}

bool GameStateReader::followBlock(u64 now) {
    u8 raw[tp::kStateBlockSize];
    tp::StateBlock block = {};
    if (R_FAILED(cheatmem::read(m_block, raw, sizeof(raw))) || !tp::readStateBlock(raw, block)) return false;
    if (block.beat != m_beat) {
        m_beat = block.beat;
        m_beatMs = now;
        m_bits = block.bits;
        publish(true, block.bits);
    } else if (now - m_beatMs > kStaleMs) {
        // The mod stands still: the game is paused or loading. Packs can have a layout for that.
        publish(true, m_bits | tp::kStateStalled);
    }
    return true;
}

// A candidate is the real thing if its counter moves: the game also keeps a still copy of the
// script it was started from.
bool GameStateReader::tryCandidate(u64 address) {
    u8 raw[tp::kStateBlockSize];
    tp::StateBlock first = {}, second = {};
    if (R_FAILED(cheatmem::read(address, raw, sizeof(raw))) || !tp::readStateBlock(raw, first)) return false;
    svcSleepThread(kLivenessNs);
    if (R_FAILED(cheatmem::read(address, raw, sizeof(raw))) || !tp::readStateBlock(raw, second)) return false;
    if (second.beat == first.beat) return false;

    const u64 now = nowMs();
    m_block = address;
    m_beat = second.beat;
    m_bits = second.bits;
    m_beatMs = now;
    m_notes = 0;
    m_misses = 0;
    publish(true, second.bits);
    logx::line("state: report found at %llX after reading %llu MB in %llu ms, flags %08X", static_cast<unsigned long long>(address),
               static_cast<unsigned long long>(m_scanned >> 20), static_cast<unsigned long long>(now - m_passStartMs), second.bits);
    return true;
}

void GameStateReader::scanSlice(u64 now) {
    for (int step = 0; step < kSliceSteps; step++) {
        if (m_cursor >= m_regionEnd) {
            // On to the next region of the game's address space.
            MemoryInfo info = {};
            const bool atEnd = m_cursor >= m_spaceEnd || R_FAILED(cheatmem::query(m_cursor, info)) || info.size == 0 ||
                               info.addr + info.size <= m_cursor;
            if (atEnd && m_heapOnly && m_cursor >= m_spaceEnd) {
                // Script variables are most likely on the heap; the rest is searched only if they are not.
                m_heapOnly = false;
                m_cursor = m_regionEnd = m_spaceBase;
                continue;
            }
            if (atEnd) {
                const int rest = m_misses < 4 ? m_misses : 4;
                m_misses++;
                if (m_notes < 3) {
                    m_notes++;
                    logx::line("state: no report in %llu MB of the game's memory (%llu ms); is the mod installed and a save loaded?",
                               static_cast<unsigned long long>(m_scanned >> 20), static_cast<unsigned long long>(now - m_passStartMs));
                }
                m_restMs = now + kRescanMs[rest];
                m_heapOnly = true;
                if (m_scanned == 0) {
                    cheatmem::close();  // nothing could be read at all: the game is gone, attach afresh next time
                } else {
                    // Stay attached and go round again: the mod starts with the story, which may load later.
                    m_cursor = m_regionEnd = m_spaceBase;
                    m_scanned = 0;
                    m_passStartMs = m_restMs;
                }
                return;
            }
            const u64 end = info.addr + info.size;
            // Script variables live in ordinary read-write memory; skip code, and anything
            // shared with devices or other processes.
            const bool heap = info.type == MemType_Heap;
            if (info.type == MemType_Unmapped || info.perm != Perm_Rw || info.attr != 0 || heap != m_heapOnly) {
                m_cursor = m_regionEnd = end;
                continue;
            }
            m_regionEnd = end;
        }

        u64 size = m_regionEnd - m_cursor;
        if (size > cheatmem::kChunk) size = cheatmem::kChunk;
        u8* data = cheatmem::chunk();
        if (R_FAILED(cheatmem::read(m_cursor, data, size))) {
            m_cursor = m_regionEnd;  // unreadable after all: leave the region
            continue;
        }
        m_scanned += size;
        for (int at = tp::findStateMarker(data, static_cast<int>(size)); at >= 0; at = tp::findStateMarker(data, static_cast<int>(size), at + 8)) {
            if (tryCandidate(m_cursor + static_cast<u64>(at))) return;
        }
        m_cursor += size;
    }
}
