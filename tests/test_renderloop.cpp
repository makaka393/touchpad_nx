#include "tinytest.hpp"
#include "../core/renderloop.hpp"

#include <vector>

using namespace tp;

namespace {

const Layout kLayout = {
    {{BtnA, Shape::Circle, 1000, 300, 50, 50, Glyph::A}},
    1,
    {200, 300, 100, 140, 80},
    {900, 560, 100, 120, 80},
    {350, 560, 100, 36, 120, 20},
    {640, 50, 30},
    14,
};

// Stand-in for the console: time only advances when the loop sleeps, and the loop is stopped
// after a fixed number of passes. `script` is the picture the input thread would publish on
// each pass (the last entry repeats).
struct FakeHost {
    std::vector<Visual> script{Visual{}};
    int passesAllowed = 0;

    int passes = 0;
    int presents = 0;
    uint64_t sleptNs = 0;
    int sleeps = 0;

    bool exitRequested() { return passes >= passesAllowed; }
    Scene latestScene() {
        const size_t i = static_cast<size_t>(passes) < script.size() ? passes : script.size() - 1;
        passes++;
        Scene scene;
        scene.layout = kLayout;
        scene.visual = script[i];
        return scene;
    }
    void present() { presents++; }
    void sleepNs(uint64_t ns) {
        sleptNs += ns;
        sleeps++;
    }
};

struct Rig {
    std::vector<uint16_t> px = std::vector<uint16_t>(640 * 360, 0);
    Canvas canvas{px.data(), 640, 360, 640};
    Renderer renderer{canvas};
};

Visual pressedA() {
    Visual v;
    v.pad.buttons = BtnA;
    return v;
}

}  // namespace

// The freeze on the console: once nothing needed repainting the loop ran flat out and
// starved the system core. Every pass must hand the core back.
TEST(render_loop_sleeps_on_every_pass_even_when_nothing_changes) {
    Rig rig;
    FakeHost host;
    host.passesAllowed = 100;
    runRenderLoop(host, rig.renderer);
    CHECK_EQ(host.passes, 100);
    CHECK_EQ(host.sleeps, 100);
    CHECK(host.sleptNs >= 100ull * 30'000'000ull);  // about three seconds of wall time, not zero
}

TEST(render_loop_presents_only_when_the_picture_changes) {
    Rig rig;
    FakeHost host;
    host.script = {Visual{}, Visual{}, Visual{}, pressedA(), pressedA(), Visual{}, Visual{}};
    host.passesAllowed = 7;
    runRenderLoop(host, rig.renderer);
    CHECK_EQ(host.presents, 3);  // first frame, press, release
}

TEST(render_loop_does_nothing_once_exit_is_requested) {
    Rig rig;
    FakeHost host;
    host.passesAllowed = 0;
    runRenderLoop(host, rig.renderer);
    CHECK_EQ(host.presents, 0);
    CHECK_EQ(host.sleeps, 0);
}
