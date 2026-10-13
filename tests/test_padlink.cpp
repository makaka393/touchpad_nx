#include "tinytest.hpp"
#include "../core/padlink.hpp"

#include <string>
#include <vector>

using namespace tp;

namespace {

// Scriptable controller backend that records every call.
struct FakeBackend {
    bool attachOk = true;
    bool sendOk = true;

    std::string calls;            // 'A' attach, 'D' detach, 'S' send
    std::vector<PadState> sent;

    bool attach() {
        calls += 'A';
        return attachOk;
    }
    void detach() { calls += 'D'; }
    bool send(const PadState& pad) {
        calls += 'S';
        if (sendOk) sent.push_back(pad);
        return sendOk;
    }
};

// Last state the controller received; a value no test expects if it received nothing.
PadState lastSent(const FakeBackend& b) {
    if (b.sent.empty()) {
        PadState none;
        none.buttons = 0xFFFFFFFFu;
        return none;
    }
    return b.sent.back();
}

PadState press(uint32_t buttons) {
    PadState p;
    p.buttons = buttons;
    return p;
}

}  // namespace

TEST(start_connects_the_controller_with_nothing_pressed) {
    FakeBackend b;
    PadLink<FakeBackend> link(b);
    CHECK(link.start(1000));
    CHECK(link.attached());
    CHECK(b.calls == "AS");
    CHECK(b.sent.size() == 1 && b.sent[0] == PadState{});
}

TEST(unchanged_state_is_not_sent_again) {
    FakeBackend b;
    PadLink<FakeBackend> link(b);
    link.start(1000);
    link.update(PadState{}, 1004);
    link.update(PadState{}, 1008);
    CHECK(b.calls == "AS");
}

TEST(changed_state_is_sent_once) {
    FakeBackend b;
    PadLink<FakeBackend> link(b);
    link.start(1000);
    link.update(press(BtnA), 1004);
    link.update(press(BtnA), 1008);
    CHECK(b.calls == "ASS");
    CHECK(lastSent(b) == press(BtnA));
}

// A game that does not take another controller makes the system drop ours: sends start failing.
TEST(rejected_send_disconnects_the_controller) {
    FakeBackend b;
    PadLink<FakeBackend> link(b);
    link.start(1000);
    b.sendOk = false;
    link.update(press(BtnA), 1004);
    CHECK(!link.attached());
    CHECK(b.calls == "ASSD");
}

TEST(disconnected_link_leaves_the_system_alone_while_nothing_is_pressed) {
    FakeBackend b;
    PadLink<FakeBackend> link(b);
    link.start(1000);
    b.sendOk = false;
    link.update(press(BtnA), 1004);
    b.calls.clear();
    for (uint64_t t = 1008; t < 20000; t += 4) link.update(PadState{}, t);
    CHECK(b.calls.empty());
}

TEST(pressing_while_disconnected_reconnects_and_delivers_the_press) {
    FakeBackend b;
    PadLink<FakeBackend> link(b);
    link.start(1000);
    b.sendOk = false;
    link.update(press(BtnA), 1004);  // dropped
    link.update(PadState{}, 1008);
    b.sendOk = true;
    b.calls.clear();
    link.update(press(BtnB), 3000);
    CHECK(link.attached());
    CHECK(b.calls == "AS");
    CHECK(lastSent(b) == press(BtnB));
}

TEST(reconnect_attempts_are_limited_to_one_a_second) {
    FakeBackend b;
    PadLink<FakeBackend> link(b);
    link.start(1000);
    b.sendOk = false;
    link.update(press(BtnA), 1004);  // dropped at t=1004
    b.attachOk = false;
    b.calls.clear();
    // Button held for three seconds while the system keeps refusing.
    for (uint64_t t = 1008; t <= 4004; t += 4) link.update(press(BtnA), t);
    CHECK(b.calls == "AAA");  // t=2000, 3000, 4000: one second after the previous attempt each
}

TEST(reconnected_controller_gets_the_state_even_if_it_matches_the_last_one_sent) {
    FakeBackend b;
    PadLink<FakeBackend> link(b);
    link.start(1000);
    link.update(press(BtnA), 1004);  // delivered
    b.sendOk = false;
    link.update(press(BtnB), 1008);  // dropped
    b.sendOk = true;
    link.update(press(BtnA), 3000);  // same as the last delivered state, but a new controller
    CHECK(lastSent(b) == press(BtnA));
    CHECK_EQ(b.sent.size(), 3);  // neutral, A, A again
}

TEST(refused_start_recovers_on_a_later_press) {
    FakeBackend b;
    b.attachOk = false;
    PadLink<FakeBackend> link(b);
    CHECK(!link.start(1000));
    b.attachOk = true;
    link.update(press(BtnA), 1500);  // too soon after the failed attempt
    CHECK(!link.attached());
    link.update(press(BtnA), 2100);
    CHECK(link.attached());
    CHECK(lastSent(b) == press(BtnA));
}

TEST(stop_releases_everything_then_disconnects) {
    FakeBackend b;
    PadLink<FakeBackend> link(b);
    link.start(1000);
    link.update(press(BtnA | BtnZR), 1004);
    b.calls.clear();
    link.stop();
    CHECK(b.calls == "SD");
    CHECK(lastSent(b) == PadState{});
    CHECK(!link.attached());
}

TEST(stop_on_a_disconnected_link_does_nothing) {
    FakeBackend b;
    b.attachOk = false;
    PadLink<FakeBackend> link(b);
    link.start(1000);
    b.calls.clear();
    link.stop();
    CHECK(b.calls.empty());
}
