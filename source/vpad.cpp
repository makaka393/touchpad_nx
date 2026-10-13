#include "vpad.hpp"

#include <cstdlib>

#include "log.hpp"

// The core's button bits are handed to HDLS unchanged, so they must match libnx exactly.
#define SAME_BIT(ours, theirs) static_assert(static_cast<u64>(ours) == static_cast<u64>(theirs), #ours " != " #theirs)
SAME_BIT(tp::BtnA, HidNpadButton_A);
SAME_BIT(tp::BtnB, HidNpadButton_B);
SAME_BIT(tp::BtnX, HidNpadButton_X);
SAME_BIT(tp::BtnY, HidNpadButton_Y);
SAME_BIT(tp::BtnStickL, HidNpadButton_StickL);
SAME_BIT(tp::BtnStickR, HidNpadButton_StickR);
SAME_BIT(tp::BtnL, HidNpadButton_L);
SAME_BIT(tp::BtnR, HidNpadButton_R);
SAME_BIT(tp::BtnZL, HidNpadButton_ZL);
SAME_BIT(tp::BtnZR, HidNpadButton_ZR);
SAME_BIT(tp::BtnPlus, HidNpadButton_Plus);
SAME_BIT(tp::BtnMinus, HidNpadButton_Minus);
SAME_BIT(tp::BtnLeft, HidNpadButton_Left);
SAME_BIT(tp::BtnUp, HidNpadButton_Up);
SAME_BIT(tp::BtnRight, HidNpadButton_Right);
SAME_BIT(tp::BtnDown, HidNpadButton_Down);
SAME_BIT(tp::BtnHome, HiddbgNpadButton_Home);
SAME_BIT(tp::BtnCapture, HiddbgNpadButton_Capture);
SAME_BIT(tp::kStickMax, JOYSTICK_MAX);
#undef SAME_BIT

namespace {
// Same size sys-con uses, which is known to work on current firmware.
constexpr size_t kWorkBufferSize = 0x8000;
}

void VirtualPad::note(const char* what, Result rc) {
    if (m_logBudget <= 0) return;
    m_logBudget--;
    logx::line("vpad: %s -> 0x%x%s", what, rc, m_logBudget == 0 ? " (further controller events not logged)" : "");
}

bool VirtualPad::init() {
    Result rc = hiddbgInitialize();
    if (R_FAILED(rc)) {
        logx::line("vpad: hiddbgInitialize failed: 0x%x", rc);
        return false;
    }
    m_serviceReady = true;

    m_workBuffer = std::aligned_alloc(0x1000, kWorkBufferSize);
    if (!m_workBuffer) {
        logx::line("vpad: work buffer allocation failed");
        return false;
    }

    rc = hiddbgAttachHdlsWorkBuffer(&m_session, m_workBuffer, kWorkBufferSize);
    if (R_FAILED(rc)) {
        logx::line("vpad: hiddbgAttachHdlsWorkBuffer failed: 0x%x", rc);
        return false;
    }
    m_sessionReady = true;
    return true;
}

void VirtualPad::exit() {
    detach();
    if (m_sessionReady) {
        hiddbgReleaseHdlsWorkBuffer(m_session);
        m_sessionReady = false;
    }
    if (m_serviceReady) {
        hiddbgExit();
        m_serviceReady = false;
    }
    std::free(m_workBuffer);
    m_workBuffer = nullptr;
}

bool VirtualPad::attach() {
    if (!m_sessionReady) return false;
    if (m_deviceReady) return true;

    HiddbgHdlsDeviceInfo info = {};
    info.deviceType = HidDeviceType_FullKey3;  // Pro Controller
    info.npadInterfaceType = HidNpadInterfaceType_USB;
    info.singleColorBody = RGBA8_MAXALPHA(45, 45, 45);
    info.singleColorButtons = RGBA8_MAXALPHA(230, 230, 230);
    info.colorLeftGrip = RGBA8_MAXALPHA(45, 45, 45);
    info.colorRightGrip = RGBA8_MAXALPHA(45, 45, 45);

    const Result rc = hiddbgAttachHdlsVirtualDevice(&m_handle, &info);
    note("attach controller", rc);
    m_deviceReady = R_SUCCEEDED(rc);
    return m_deviceReady;
}

void VirtualPad::detach() {
    if (!m_deviceReady) return;
    const Result rc = hiddbgDetachHdlsVirtualDevice(m_handle);
    note("detach controller", rc);
    m_deviceReady = false;
}

bool VirtualPad::send(const tp::PadState& pad) {
    if (!m_deviceReady) return false;

    HiddbgHdlsState state = {};
    state.battery_level = 4;  // full
    state.buttons = pad.buttons;
    state.analog_stick_l.x = pad.lx;
    state.analog_stick_l.y = pad.ly;
    state.analog_stick_r.x = pad.rx;
    state.analog_stick_r.y = pad.ry;

    const Result rc = hiddbgSetHdlsState(m_handle, &state);
    if (R_FAILED(rc)) note("state rejected (controller dropped by the system?)", rc);
    return R_SUCCEEDED(rc);
}
