// Virtual Pro Controller through hid:dbg HDLS (the mechanism sys-botbase and sys-con use).
// Serves as the backend of tp::PadLink, which decides when to attach, send and give up.
#pragma once
#include <switch.h>

#include "../core/types.hpp"

class VirtualPad {
public:
    bool init();  // opens the service and the HDLS session; no controller yet
    void exit();

    bool attach();
    void detach();
    bool send(const tp::PadState& pad);

private:
    void note(const char* what, Result rc);

    HiddbgHdlsSessionId m_session{};
    HiddbgHdlsHandle m_handle{};
    void* m_workBuffer = nullptr;
    bool m_serviceReady = false;
    bool m_sessionReady = false;
    bool m_deviceReady = false;
    int m_logBudget = 24;  // attach/drop events worth recording before going quiet
};
