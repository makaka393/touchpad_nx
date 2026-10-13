// Keeps the virtual controller connected and in step with the pad state.
#pragma once
#include "types.hpp"

namespace tp {

// Backend provides:
//   bool attach();                  // connect the controller; false if the system refused
//   void detach();
//   bool send(const PadState&);     // false if the controller is no longer accepted
template <typename Backend>
class PadLink {
public:
    explicit PadLink(Backend& backend) : m_backend(backend) {}

    bool start(uint64_t nowMs);
    // Call on every input pass with the current state.
    void update(const PadState& pad, uint64_t nowMs);
    void stop();
    bool attached() const { return m_attached; }

private:
    // While a game refuses the controller, ask again at most this often, and only while
    // something is actually pressed: every attempt makes the system announce a new controller.
    static constexpr uint64_t kRetryMs = 1000;

    Backend& m_backend;
    bool m_attached = false;
    bool m_resend = false;        // a freshly attached controller knows nothing yet
    uint64_t m_lastAttemptMs = 0;
    PadState m_sent;
};

template <typename Backend>
bool PadLink<Backend>::start(uint64_t nowMs) {
    m_lastAttemptMs = nowMs;
    m_attached = m_backend.attach();
    m_resend = m_attached;
    if (m_attached) update(PadState{}, nowMs);
    return m_attached;
}

template <typename Backend>
void PadLink<Backend>::update(const PadState& pad, uint64_t nowMs) {
    if (!m_attached) {
        if (pad == PadState{}) return;
        if (nowMs - m_lastAttemptMs < kRetryMs) return;
        m_lastAttemptMs = nowMs;
        if (!m_backend.attach()) return;
        m_attached = true;
        m_resend = true;
    }

    if (!m_resend && pad == m_sent) return;

    if (!m_backend.send(pad)) {
        // The system took the controller away (game accepts no more pads, sleep, ...).
        m_backend.detach();
        m_attached = false;
        m_lastAttemptMs = nowMs;
        return;
    }
    m_sent = pad;
    m_resend = false;
}

template <typename Backend>
void PadLink<Backend>::stop() {
    if (!m_attached) return;
    m_backend.send(PadState{});  // never leave a button held behind us
    m_backend.detach();
    m_attached = false;
}

}  // namespace tp
