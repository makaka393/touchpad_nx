#include "touchgate.hpp"

#include "log.hpp"

namespace {

// hid:sys EnableAppletToGetTouchScreen [14.0.0+]: like EnableAppletToGetInput (cmd 503, what
// Tesla uses to grab focus) but for the touch screen only, so pad input keeps flowing.
Result enableAppletToGetTouchScreen(bool enable, u64 aruid) {
    const struct {
        u8 permit;
        u64 aruid;
    } in = {enable, aruid};
    return serviceDispatchIn(hidsysGetServiceSession(), 507, in);
}

}  // namespace

void TouchGate::apply(bool block) {
    if (hosversionBefore(14, 0, 0)) {
        if (!m_logged) logx::line("touchgate: firmware older than 14.0.0, the game will also see touches");
        m_logged = true;
        return;
    }

    u64 pid = 0;
    if (R_FAILED(pmdmntGetApplicationProcessId(&pid))) pid = 0;

    if (m_blockedPid != 0 && (!block || pid != m_blockedPid)) {
        enableAppletToGetTouchScreen(true, m_blockedPid);
        m_blockedPid = 0;
    }
    if (block && pid != 0) {
        const Result rc = enableAppletToGetTouchScreen(false, pid);
        if (!m_logged) {
            logx::line("touchgate: block touch for pid %llu -> 0x%x", static_cast<unsigned long long>(pid), rc);
            m_logged = true;
        }
        if (R_SUCCEEDED(rc)) m_blockedPid = pid;
    }
}
