#include "layer.hpp"

#include "log.hpp"

extern "C" u64 __nx_vi_layer_id;

namespace {

constexpr s32 kDisplayW = 1920;  // vi positions layers in 1080p coordinates
constexpr s32 kDisplayH = 1080;

Result addToLayerStack(ViLayer* layer, ViLayerStack stack) {
    const struct {
        u32 stack;
        u64 layerId;
    } in = {stack, layer->layer_id};
    return serviceDispatchIn(viGetSession_IManagerDisplayService(), 6000, in);
}

bool step(const char* what, Result rc) {
    if (R_FAILED(rc)) logx::line("layer: %s failed: 0x%x", what, rc);
    return R_SUCCEEDED(rc);
}

}  // namespace

// Same sequence libtesla / libultrahand use for their overlay layer.
bool Layer::init() {
    if (!step("viInitialize", viInitialize(ViServiceType_Manager))) return false;
    m_viReady = true;
    if (!step("viOpenDefaultDisplay", viOpenDefaultDisplay(&m_display))) return false;
    m_displayOpen = true;
    if (!step("viCreateManagedLayer", viCreateManagedLayer(&m_display, static_cast<ViLayerFlags>(0), 0, &__nx_vi_layer_id))) return false;
    if (!step("viCreateLayer", viCreateLayer(&m_display, &m_layer))) return false;
    m_layerReady = true;
    if (!step("viSetLayerScalingMode", viSetLayerScalingMode(&m_layer, ViScalingMode_FitToLayer))) return false;

    s32 z = 0;
    if (R_FAILED(viGetZOrderCountMax(&m_display, &z)) || z <= 0) z = 255;
    if (!step("viSetLayerZ", viSetLayerZ(&m_layer, z))) return false;

    if (!step("layer stack Default", addToLayerStack(&m_layer, ViLayerStack_Default))) return false;
    // Deliberately not in the Screenshot / Recording / LastFrame stacks: captures stay clean.
    step("layer stack Arbitrary", addToLayerStack(&m_layer, ViLayerStack_Arbitrary));
    step("layer stack Null", addToLayerStack(&m_layer, ViLayerStack_Null));
    step("layer stack ApplicationForDebug", addToLayerStack(&m_layer, ViLayerStack_ApplicationForDebug));
    step("layer stack Lcd", addToLayerStack(&m_layer, ViLayerStack_Lcd));

    if (!step("viSetLayerSize", viSetLayerSize(&m_layer, kDisplayW, kDisplayH))) return false;
    if (!step("viSetLayerPosition", viSetLayerPosition(&m_layer, 0, 0))) return false;
    if (!step("nwindowCreateFromLayer", nwindowCreateFromLayer(&m_window, &m_layer))) return false;
    m_windowReady = true;

    if (createBuffers(kSharpWidth, kSharpHeight)) return true;
    logx::line("layer: no room for %dx%d, falling back to %dx%d", kSharpWidth, kSharpHeight, kPlainWidth, kPlainHeight);
    return createBuffers(kPlainWidth, kPlainHeight);
}

bool Layer::createBuffers(int width, int height) {
    if (!step("framebufferCreate", framebufferCreate(&m_fb, &m_window, width, height, PIXEL_FORMAT_RGBA_4444, 2))) return false;
    if (!step("framebufferMakeLinear", framebufferMakeLinear(&m_fb))) {
        framebufferClose(&m_fb);
        return false;
    }
    m_fbReady = true;
    m_width = width;
    m_height = height;
    return true;
}

void Layer::exit() {
    if (m_fbReady) framebufferClose(&m_fb);
    if (m_windowReady) nwindowClose(&m_window);
    if (m_layerReady) viDestroyManagedLayer(&m_layer);
    if (m_displayOpen) viCloseDisplay(&m_display);
    if (m_viReady) viExit();
    m_fbReady = m_windowReady = m_layerReady = m_displayOpen = m_viReady = false;
}

void Layer::present() {
    // In linear mode libnx keeps one persistent shadow buffer (what pixels() returns) and
    // converts it into the next swap-chain buffer here, so partial repaints are enough.
    framebufferBegin(&m_fb, nullptr);
    framebufferEnd(&m_fb);
}
