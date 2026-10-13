#include "gameprobe.hpp"

#include <cstdio>
#include <sys/stat.h>

#include "cheatmem.hpp"
#include "log.hpp"

namespace gameprobe {

namespace {

using cheatmem::kChunk;
using cheatmem::query;
using cheatmem::read;

constexpr const char* kSwitchFile = "sdmc:/config/touchpad/probe.txt";
constexpr const char* kScratchFile = "sdmc:/config/touchpad/probe.tmp";

u64 nowUs() { return armTicksToNs(armGetSystemTick()) / 1000; }

// Walks the regions from `from` up to `to` and adds up their sizes by access rights.
void survey(u64 from, u64 to, const char* what) {
    u64 code = 0, readOnly = 0, writable = 0, largest = 0;
    int regions = 0, writableRegions = 0;
    u64 at = from;
    for (int guard = 0; guard < 20000 && at < to; guard++) {
        MemoryInfo info = {};
        if (R_FAILED(query(at, info)) || info.size == 0) break;
        const u64 end = info.addr + info.size;
        if (info.type != MemType_Unmapped) {
            regions++;
            if (info.perm == Perm_Rx) code += info.size;
            else if (info.perm == Perm_R) readOnly += info.size;
            else if (info.perm == Perm_Rw) {
                writable += info.size;
                writableRegions++;
                if (info.size > largest) largest = info.size;
            }
        }
        if (end <= at) break;  // wrapped around the top of the address space
        at = end;
    }
    logx::line("probe: %s: %d regions, code %llu KB, read-only %llu KB, writable %llu KB in %d regions (largest %llu KB)", what,
               regions, static_cast<unsigned long long>(code >> 10), static_cast<unsigned long long>(readOnly >> 10),
               static_cast<unsigned long long>(writable >> 10), writableRegions, static_cast<unsigned long long>(largest >> 10));
}

// First writable region at or after `from`, below `to`. Size 0 when there is none.
MemoryInfo firstWritable(u64 from, u64 to) {
    u64 at = from;
    for (int guard = 0; guard < 20000 && at < to; guard++) {
        MemoryInfo info = {};
        if (R_FAILED(query(at, info)) || info.size == 0) break;
        if (info.type != MemType_Unmapped && info.perm == Perm_Rw && info.size >= kChunk) return info;
        if (info.addr + info.size <= at) break;
        at = info.addr + info.size;
    }
    return MemoryInfo{};
}

void timeReads(const MemoryInfo& region) {
    if (region.size == 0) {
        logx::line("probe: no writable region to time reads on");
        return;
    }
    // Bulk: up to 4 MB in chunks, the way a scan would read.
    u64 want = region.size < (4u << 20) ? region.size : (4u << 20);
    want -= want % kChunk;
    u64 done = 0;
    Result rc = 0;
    const u64 bulkStart = nowUs();
    for (; done < want; done += kChunk) {
        if (R_FAILED(rc = read(region.addr + done, cheatmem::chunk(), kChunk))) break;
    }
    const u64 bulkUs = nowUs() - bulkStart;
    logx::line("probe: read %llu KB in %llu ms (last result 0x%x)", static_cast<unsigned long long>(done >> 10),
               static_cast<unsigned long long>(bulkUs / 1000), rc);

    // Single values: what watching a handful of addresses costs.
    u64 value = 0;
    int ok = 0;
    const u64 smallStart = nowUs();
    for (int i = 0; i < 500; i++) ok += R_SUCCEEDED(read(region.addr + (static_cast<u64>(i) * 0x40) % want, &value, sizeof(value)));
    const u64 smallUs = nowUs() - smallStart;
    logx::line("probe: 500 eight-byte reads in %llu us, %d succeeded", static_cast<unsigned long long>(smallUs), ok);
}

void timeCardWrites() {
    FILE* f = std::fopen(kScratchFile, "wb");
    if (!f) {
        logx::line("probe: could not create a scratch file on the card");
        return;
    }
    const u64 start = nowUs();
    size_t written = 0;
    for (int i = 0; i < 64; i++) written += std::fwrite(cheatmem::chunk(), 1, kChunk, f);  // 2 MB
    std::fclose(f);
    const u64 us = nowUs() - start;
    std::remove(kScratchFile);
    logx::line("probe: wrote %llu KB to the card in %llu ms", static_cast<unsigned long long>(written >> 10),
               static_cast<unsigned long long>(us / 1000));
}

}  // namespace

void report() {
    struct stat st;
    if (stat(kSwitchFile, &st) != 0) return;

    u64 pid = 0;
    if (R_FAILED(pmdmntGetApplicationProcessId(&pid))) {
        logx::line("probe: no game is running, nothing to look at");
        return;
    }

    Result rc = 0;
    if (!cheatmem::open(&rc)) {
        logx::line("probe: the cheat service cannot look into the game: 0x%x", rc);
        return;
    }
    logx::line("probe: cheat service attached to the game");

    cheatmem::Metadata meta = {};
    rc = cheatmem::metadata(meta);
    if (R_FAILED(rc)) {
        logx::line("probe: no process information: 0x%x", rc);
    } else {
        char build[41];
        for (int i = 0; i < 20; i++) std::snprintf(build + i * 2, 3, "%02X", meta.buildId[i]);
        logx::line("probe: game %016llX build %s", static_cast<unsigned long long>(meta.titleId), build);
        logx::line("probe: main %llX + %llu KB, heap %llX + %llu KB, alias %llX + %llu KB", static_cast<unsigned long long>(meta.main.base),
                   static_cast<unsigned long long>(meta.main.size >> 10), static_cast<unsigned long long>(meta.heap.base),
                   static_cast<unsigned long long>(meta.heap.size >> 10), static_cast<unsigned long long>(meta.alias.base),
                   static_cast<unsigned long long>(meta.alias.size >> 10));

        survey(meta.main.base, meta.main.base + meta.main.size, "main module");
        survey(meta.addressSpace.base, meta.addressSpace.base + meta.addressSpace.size, "whole process");
        timeReads(firstWritable(meta.main.base, meta.main.base + meta.main.size));
        timeCardWrites();
    }

    cheatmem::close();
}

}  // namespace gameprobe
