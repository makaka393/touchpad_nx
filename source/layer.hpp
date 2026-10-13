// Full-screen display layer above the running game, backed by an RGBA4444 buffer that the
// compositor scales up to the screen.
#pragma once
#include <switch.h>

class Layer {
public:
    // 896x504 is the sharpest picture the smallest (4 MB) overlay heap has room for: two
    // swap-chain buffers and the linear one come to about 2.8 MB. If that cannot be had, the
    // layer settles for 640x360 (about 1.4 MB), which is what 0.2 and 0.3 always used.
    static constexpr int kSharpWidth = 896, kSharpHeight = 504;
    static constexpr int kPlainWidth = 640, kPlainHeight = 360;

    bool init();
    void exit();

    int width() const { return m_width; }
    int height() const { return m_height; }

    u16* pixels() { return static_cast<u16*>(m_fb.buf_linear); }
    int stridePixels() const { return static_cast<int>(m_fb.stride / sizeof(u16)); }

    // Sends the current contents of pixels() to the screen.
    void present();

private:
    ViDisplay m_display{};
    ViLayer m_layer{};
    NWindow m_window{};
    Framebuffer m_fb{};
    bool m_viReady = false;
    bool m_displayOpen = false;
    bool m_layerReady = false;
    bool m_windowReady = false;
    bool m_fbReady = false;
    int m_width = 0;
    int m_height = 0;

    bool createBuffers(int width, int height);
};
