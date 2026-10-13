#include "cheatmem.hpp"

namespace cheatmem {

namespace {

Service g_service;
bool g_open = false;
bool g_weAttached = false;
u8 g_chunk[kChunk];

Result hasProcess(bool& out) {
    u8 value = 0;
    const Result rc = serviceDispatchOut(&g_service, 65000, value);
    out = value != 0;
    return rc;
}

Result forceOpen() { return serviceDispatch(&g_service, 65003); }
Result forceClose() { return serviceDispatch(&g_service, 65006); }

}  // namespace

bool open(Result* result) {
    if (g_open) return true;
    Result rc = smGetService(&g_service, "dmnt:cht");
    if (R_SUCCEEDED(rc)) {
        bool attached = false;
        rc = hasProcess(attached);
        g_weAttached = false;
        if (R_SUCCEEDED(rc) && !attached) {
            rc = forceOpen();
            g_weAttached = R_SUCCEEDED(rc);
        }
        if (R_FAILED(rc)) serviceClose(&g_service);
    }
    if (result) *result = rc;
    g_open = R_SUCCEEDED(rc);
    return g_open;
}

void close() {
    if (!g_open) return;
    if (g_weAttached) forceClose();
    serviceClose(&g_service);
    g_open = false;
    g_weAttached = false;
}

bool isOpen() { return g_open; }

Result metadata(Metadata& out) { return serviceDispatchOut(&g_service, 65002, out); }

Result query(u64 address, MemoryInfo& out) { return serviceDispatchInOut(&g_service, 65104, address, out); }

Result read(u64 address, void* buffer, size_t size) {
    const struct {
        u64 address;
        u64 size;
    } in = {address, size};
    return serviceDispatchIn(&g_service, 65102, in, .buffer_attrs = {SfBufferAttr_Out | SfBufferAttr_HipcMapAlias},
                             .buffers = {{buffer, size}});
}

u8* chunk() { return g_chunk; }

}  // namespace cheatmem
