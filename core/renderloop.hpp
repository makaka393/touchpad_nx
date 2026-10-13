// The render thread's loop, written against a small host interface so its pacing can be tested
// off the console.
#pragma once
#include "render.hpp"

namespace tp {

constexpr uint64_t kFrameNs = 33'000'000;  // at most ~30 repaints a second

// Host provides:
//   bool exitRequested();
//   Scene latestScene();
//   void present();            // push the canvas to the screen
//   void sleepNs(uint64_t ns);
template <typename Host>
void runRenderLoop(Host& host, Renderer& renderer) {
    while (!host.exitRequested()) {
        if (renderer.draw(host.latestScene())) host.present();
        // Unconditional. A pass that never blocks turns this thread into a busy loop, and a
        // busy sysmodule thread starves everything at its priority on the system core: the
        // console freezes while audio keeps playing.
        host.sleepNs(kFrameNs);
    }
}

}  // namespace tp
