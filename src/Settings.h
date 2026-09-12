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
        bool glowPulse    = true;  // crossfade aroused8 <-> aroused9 (glow) at max arousal
        bool anOverlays   = true;  // Advanced Nudity flash overlays (ass/boobs/vagina)
        bool npcCrosshair = true;  // Shift + crosshair NPC -> peek their arousal for 5s
    };

    // 2026-09-12: TDF Enhanced Prostitution auto-whoring indicator. A second, independently
    // placed/scaled icon widget that shows only while BB_PlayerAutoWhoreEnabled == 1.
    struct WhoringConfig : WidgetConfig {};

    struct Config {
        ArousalConfig arousal{};
        WhoringConfig whoring{};
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
            whoring.x = 50.0f;  whoring.y = 130.0f;
            whoring.iconHeightPx = 48.0f; whoring.textSizePx = 18.0f; whoring.showText = false;
        }
    };

    std::unique_lock<std::mutex> Lock();
    Config& Get();

    void Load();
    void Save();

    void MarkDirty();
    bool TakeDirty();
}
