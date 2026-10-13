// TouchPad overlay: on-screen gamepad for the Switch touch screen.
//
// Two threads, and both give the core back on every pass (a sysmodule thread that never
// blocks freezes the console):
//   input  - polls the touch screen every 4 ms, maps fingers to a pad state and feeds the
//            virtual controller. Never waits on the display.
//   main   - checks ~30 times a second whether the picture changed and repaints if so.
#include <switch.h>

#include <atomic>

#include "../core/app.hpp"
#include "../core/padlink.hpp"
#include "../core/render.hpp"
#include "../core/renderloop.hpp"
#include "gameprobe.hpp"
#include "gamestate.hpp"
#include "layer.hpp"
#include "log.hpp"
#include "storage.hpp"
#include "touchgate.hpp"
#include "vpad.hpp"

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

extern "C" {

// Overlays run inside the nx-ovlloader sysmodule, not as an applet.
u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;
u32 __nx_nv_transfermem_size = 0x15000;
ViLayerFlags __nx_vi_stray_layer_flags = static_cast<ViLayerFlags>(0);

void __appInit(void) {
    Result rc;
    if (R_FAILED(rc = smInitialize())) diagAbortWithResult(rc);
    if (R_FAILED(rc = fsInitialize())) diagAbortWithResult(rc);
    if (R_FAILED(rc = hidInitialize())) diagAbortWithResult(rc);
    if (R_FAILED(rc = pmdmntInitialize())) diagAbortWithResult(rc);
    if (R_FAILED(rc = hidsysInitialize())) diagAbortWithResult(rc);
    fsdevMountSdmc();
}

void __appExit(void) {
    fsdevUnmountAll();
    fsExit();
    hidExit();
    pmdmntExit();
    hidsysExit();
    smExit();
}

}  // extern "C"

namespace {

constexpr u64 kPollNs = 4'000'000;          // touch is sampled about every 4 ms
constexpr u64 kHousekeepingMs = 500;
constexpr u32 kHeartbeatPasses = 150;       // render passes between heartbeat lines (~5 s)
constexpr u32 kHeartbeatLines = 120;        // stop after ~10 minutes

struct Shared {
    Mutex lock = 0;             // guards scene
    tp::Scene scene;
    std::atomic<bool> stop{false};
    std::atomic<bool> exitRequested{false};
    std::atomic<bool> gameEnded{false};  // leaving because the game closed, not because the player asked
    // Diagnostics for the heartbeat.
    std::atomic<u32> inputPasses{0};
    std::atomic<u32> fingers{0};
    std::atomic<bool> padAttached{false};
    VirtualPad* pad = nullptr;
    TouchGate* gate = nullptr;
    GameStateReader* state = nullptr;
    u32 ultrahandCombo = 0;
    u64 game = 0;               // title the app was set up for before the input thread started
};

// Set up by main, then owned by the input thread; too big for a stack.
tp::App g_app;

u64 nowMs() { return armTicksToNs(armGetSystemTick()) / 1'000'000; }

// Title id of the running game, 0 when there is none.
u64 runningGame() {
    u64 pid = 0, titleId = 0;
    if (R_FAILED(pmdmntGetApplicationProcessId(&pid)) || R_FAILED(pmdmntGetProgramId(&titleId, pid))) return 0;
    return titleId;
}

void inputThread(void* arg) {
    Shared& shared = *static_cast<Shared*>(arg);
    tp::App& app = g_app;
    tp::PadLink<VirtualPad> link(*shared.pad);

    u64 game = shared.game;

    // Ultrahand's own combo cannot work while this overlay is the one loaded, so we watch for
    // it ourselves (on real controllers and on our own buttons) and hand over.
    const u32 combo = shared.ultrahandCombo;
    padConfigureInput(2, HidNpadStyleSet_NpadStandard | HidNpadStyleTag_NpadSystemExt);
    ::PadState realP1, realHandheld;
    padInitialize(&realP1, HidNpadIdType_No1);
    padInitialize(&realHandheld, HidNpadIdType_Handheld);
    bool comboArmed = false;  // must be seen released once, so a held launch combo cannot bounce us

    HidTouchScreenState screen = {};
    tp::TouchPoint points[16];
    tp::Visual shownVisual;
    u32 shownRevision = 0;
    u64 lastHousekeeping = 0;
    bool gateBlocking = false;
    bool touchSeen = false;
    bool stateShown = false;
    int goneChecks = 0;
    // The finger that tapped the overlay's name in the menu is usually still down when we
    // start; wait for a clean screen so it cannot press whatever happens to be under it.
    bool armed = false;

    logx::line("input: thread up, controller %s", link.start(nowMs()) ? "attached" : "refused for now");

    while (!shared.stop.load(std::memory_order_relaxed)) {
        const u64 now = nowMs();

        int count = 0;
        if (hidGetTouchScreenStates(&screen, 1) > 0) {
            count = screen.count;
            if (count < 0) count = 0;
            if (count > 16) count = 16;
        }
        if (!armed) {
            armed = count == 0;
            count = 0;
        }
        for (int i = 0; i < count; i++) {
            const HidTouchState& t = screen.touches[i];
            points[i] = tp::TouchPoint{t.finger_id, static_cast<int>(t.x), static_cast<int>(t.y)};
        }
        if (count > 0 && !touchSeen) {
            touchSeen = true;
            logx::line("input: first touch at %d,%d", points[0].x, points[0].y);
        }

        // What the game's mod says about the player, if there is one and the pack cares.
        u32 stateBits = 0;
        const bool stateKnown = shared.state->latest(stateBits);
        if (stateKnown != stateShown) {
            stateShown = stateKnown;
            logx::line("input: the game %s its state", stateKnown ? "reports" : "no longer reports");
        }
        app.setGameState(stateKnown, stateBits);
        shared.state->setWanted(app.wantsGameState());

        app.update(points, count, now);
        link.update(app.pad(), now);

        padUpdate(&realP1);
        padUpdate(&realHandheld);
        const u64 held = padGetButtons(&realP1) | padGetButtons(&realHandheld) | app.pad().buttons;
        const bool comboHeld = (held & combo) == combo;
        if (!comboHeld) comboArmed = true;

        if (app.takeExitRequest() || (comboHeld && comboArmed)) {
            shared.exitRequested.store(true);
            break;
        }

        // Publish the picture when anything visible changed.
        const tp::Visual visual = app.visual();
        if (app.revision() != shownRevision || !(visual == shownVisual)) {
            shownRevision = app.revision();
            shownVisual = visual;
            mutexLock(&shared.lock);
            app.fillScene(shared.scene);
            mutexUnlock(&shared.lock);
        }

        // Writes are rare (a menu choice, a saved layout) and short.
        if (app.takeSettingsDirty()) storage::saveSettings(app.settings());
        const int toSave = app.takePackToSave();
        if (toSave >= 0) storage::savePack(app.packs().items[toSave]);

        // The touch block is re-asserted periodically because the system resets it when the
        // game regains focus; hiding the pad lifts it immediately.
        const bool wantBlock = app.blockGameTouch();
        if (wantBlock != gateBlocking || now - lastHousekeeping >= kHousekeepingMs) {
            shared.gate->apply(wantBlock);
            gateBlocking = wantBlock;
            if (now - lastHousekeeping >= kHousekeepingMs) {
                lastHousekeeping = now;
                const u64 current = runningGame();
                // Twice in a row, so one failed lookup cannot close the pad under a running game.
                goneChecks = (game != 0 && current == 0) ? goneChecks + 1 : 0;
                if (goneChecks >= 2) {
                    // The game is over, and so is the pad's job: nobody wants buttons over the
                    // HOME menu. (Started with no game running, the pad stays and waits for one.)
                    logx::line("input: game %016llX closed, leaving", static_cast<unsigned long long>(game));
                    shared.gameEnded.store(true);
                    shared.exitRequested.store(true);
                    break;
                }
                if (current != game && goneChecks == 0) {
                    game = current;
                    app.setGame(game);
                    logx::line("input: game %016llX, pack %s", static_cast<unsigned long long>(game),
                               app.packs().items[app.currentPack()].id);
                }
            }
        }

        shared.inputPasses.fetch_add(1, std::memory_order_relaxed);
        shared.fingers.store(static_cast<u32>(count), std::memory_order_relaxed);
        shared.padAttached.store(link.attached(), std::memory_order_relaxed);

        svcSleepThread(kPollNs);
    }

    link.stop();
}

// What tp::runRenderLoop needs from the console.
struct RenderHost {
    Shared& shared;
    Layer& layer;
    u32 passes = 0;
    u32 presents = 0;
    u32 heartbeats = 0;

    bool exitRequested() { return shared.exitRequested.load(); }

    tp::Scene latestScene() {
        mutexLock(&shared.lock);
        const tp::Scene scene = shared.scene;
        mutexUnlock(&shared.lock);
        return scene;
    }

    void present() {
        layer.present();
        presents++;
    }

    void sleepNs(u64 ns) {
        svcSleepThread(ns);
        // Heartbeat from this (low priority) thread: if the console ever hangs again, the
        // last line says how long both threads stayed alive and what they were doing.
        if (++passes % kHeartbeatPasses == 0 && heartbeats < kHeartbeatLines) {
            heartbeats++;
            logx::line("alive: render passes %u, presents %u, input passes %u, fingers %u, controller %s", passes,
                       presents, shared.inputPasses.load(), shared.fingers.load(),
                       shared.padAttached.load() ? "attached" : "detached");
        }
    }
};

}  // namespace

int main(int, char**) {
    logx::begin();
    const u32 hos = hosversionGet();
    logx::line("TouchPad %s starting: HOS %u.%u.%u, overlay heap %llu KB", APP_VERSION, HOSVER_MAJOR(hos),
               HOSVER_MINOR(hos), HOSVER_MICRO(hos), static_cast<unsigned long long>(envGetHeapOverrideSize() / 1024));

    Layer layer;
    if (!layer.init()) {
        layer.exit();
        logx::line("giving up: could not create the display layer");
        return 0;
    }
    logx::line("main: display layer ready, %dx%d", layer.width(), layer.height());

    VirtualPad pad;
    if (!pad.init()) {
        pad.exit();
        layer.exit();
        logx::line("giving up: could not open the virtual controller session");
        return 0;
    }
    logx::line("main: controller session ready");

    // Sysmodule-side HID clients share applet resource user id 0; make sure it receives input
    // even while a game has focus (the overlay menu normally leaves it enabled).
    hidsysEnableAppletToGetInput(true);
    hidInitializeTouchScreen();

    // The system's own font, for the menu (it covers Cyrillic). Without it the menu still
    // works, just without labels.
    static tp::Font font;
    PlFontData fontData = {};
    Result fontRc = plInitialize(hosversionAtLeast(16, 0, 0) ? PlServiceType_User : PlServiceType_System);
    const bool plReady = R_SUCCEEDED(fontRc);
    if (plReady) fontRc = plGetSharedFontByType(&fontData, PlSharedFontType_Standard);
    if (R_SUCCEEDED(fontRc)) font.load(static_cast<const unsigned char*>(fontData.address));
    logx::line("main: system font %s (0x%x)", font.ready() ? "ready" : "unavailable", fontRc);

    TouchGate gate;
    static GameStateReader state;
    static Shared shared;
    shared.pad = &pad;
    shared.gate = &gate;
    shared.state = &state;

    // Settings, packs and the pack of the running game are in place before the first frame,
    // so the pad appears in its final shape straight away.
    storage::load(g_app);
    shared.game = runningGame();
    g_app.setGame(shared.game);
    shared.ultrahandCombo = storage::ultrahandCombo();
    g_app.fillScene(shared.scene);
    logx::line("main: game %016llX, pack %s, Ultrahand combo mask 0x%x", static_cast<unsigned long long>(shared.game),
               g_app.packs().items[g_app.currentPack()].id, shared.ultrahandCombo);

    tp::Canvas canvas(layer.pixels(), layer.width(), layer.height(), layer.stridePixels());
    canvas.setPremultiplied(true);
    static tp::Renderer renderer(canvas, &font);

    Thread thread;
    Result rc = threadCreate(&thread, inputThread, &shared, nullptr, 0x10000, 0x2C, -2);
    if (R_SUCCEEDED(rc)) {
        rc = threadStart(&thread);
        if (R_FAILED(rc)) threadClose(&thread);
    }
    if (R_FAILED(rc)) {
        logx::line("giving up: input thread failed: 0x%x", rc);
        if (plReady) plExit();
        pad.exit();
        layer.exit();
        return 0;
    }

    state.start();  // the pad works without it; a failure is in the log

    RenderHost host{shared, layer};
    tp::runRenderLoop(host, renderer);

    shared.stop.store(true);
    threadWaitForExit(&thread);
    threadClose(&thread);
    state.stop();

    gate.restore();
    pad.exit();
    if (plReady) plExit();
    layer.exit();

    // The pad is gone from the screen and from the game: a safe moment for the memory survey.
    gameprobe::report();

    // When the player leaves the overlay, that means "give me Ultrahand": its menu is loaded
    // and shows itself right away. When the game closed, Ultrahand comes back the usual quiet
    // way and waits for its combo.
    const bool quiet = shared.gameEnded.load();
    envSetNextLoad("sdmc:/switch/.overlays/ovlmenu.ovl", quiet ? "ovlmenu.ovl" : "ovlmenu.ovl --skipCombo");
    logx::line("closed normally after %u render passes, handing over to Ultrahand%s", host.passes, quiet ? " (hidden)" : "");
    return 0;
}
