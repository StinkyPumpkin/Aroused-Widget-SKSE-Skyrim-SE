#pragma once
#include <atomic>
#include <mutex>
#include <string>

namespace Settings {

    struct WidgetConfig {
        bool  enabled       = true;
        float x             = 50.0f;
        float y             = 50.0f;
        float iconHeightPx  = 64.0f;
        float textSizePx    = 18.0f;
        bool  showText      = true;
    };

    struct ArousalConfig : WidgetConfig {
        int imageSet = 0;        // 0 = aroused (heart), 1 = exposure (drop)
    };

    struct Config {
        ArousalConfig arousal{};
        int arousalCadenceSec = 5;
        // DirectInput scan code for the manual hide-toggle key.
        // 14 = Backspace (matches iHUD's typical default).
        int hideHotkeyDX = 14;
        // Mirror the vanilla compass _alpha — when something else hides the
        // compass (iHUD, Sandbox When Idle, etc.), our widgets follow.
        bool followCompassHide = true;

        Config() {
            arousal.x = 50.0f;  arousal.y = 50.0f;
            arousal.iconHeightPx = 64.0f; arousal.textSizePx = 18.0f;
        }
    };

    std::unique_lock<std::mutex> Lock();
    Config& Get();

    void Load();
    void Save();

    void MarkDirty();
    bool TakeDirty();
}
