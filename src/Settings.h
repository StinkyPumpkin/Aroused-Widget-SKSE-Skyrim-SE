#pragma once
#include <array>
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
        // 0.4.0: 0..100 %, multiplies everything the widget draws. Configs saved before
        // 0.4.0 have no "opacity" key and load at 100.
        float opacityPct    = 100.0f;
    };

    struct ArousalConfig : WidgetConfig {
        bool glowPulse    = true;  // crossfade aroused8 <-> aroused9 (glow) at max arousal
        bool anOverlays   = true;  // Advanced Nudity flash overlays (ass/boobs/vagina)
        bool npcCrosshair = true;  // Shift + crosshair NPC -> peek their arousal for 5s
        // 0.4.0 (Nexus request): draw the three AN overlays as their own widgets, each
        // placed/scaled via Config::anWidgets, instead of on top of the icon.
        bool anSeparate   = false;
    };

    // 2026-09-12: TDF Enhanced Prostitution auto-whoring indicator. A second, independently
    // placed/scaled icon widget that shows only while BB_PlayerAutoWhoreEnabled == 1.
    struct WhoringConfig : WidgetConfig {};

    // 0.4.0: Advanced Nudity overlay widgets (used when arousal.anSeparate). Index order
    // matches HudUI's overlay table: 0 = ass, 1 = boobs, 2 = vagina.
    enum ANRegion : int { kANAss = 0, kANBoobs = 1, kANVagina = 2, kANCount = 3 };

    struct Config {
        ArousalConfig arousal{};
        WhoringConfig whoring{};
        std::array<WidgetConfig, kANCount> anWidgets{};
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
            // A row to the right of the default icon (64 px + 8 px gap), same size as the
            // icon so each piece of overlay art keeps its on-icon scale. SettingsUI re-seeds
            // this row next to the CURRENT icon the first time "separate" is switched on.
            for (int i = 0; i < kANCount; ++i) {
                auto& w = anWidgets[i];
                w.x = arousal.x + (i + 1) * (arousal.iconHeightPx + 8.0f);
                w.y = arousal.y;
                w.iconHeightPx = arousal.iconHeightPx;
                w.showText = false;
            }
        }
    };

    // config.json block names of Config::anWidgets, same index order.
    inline constexpr const char* kANKeys[kANCount] = { "anAss", "anBoobs", "anVagina" };

    std::unique_lock<std::mutex> Lock();
    Config& Get();

    void Load();
    void Save();

    void MarkDirty();
    bool TakeDirty();
}
