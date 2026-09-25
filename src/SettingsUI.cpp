#include "SettingsUI.h"
#include "HudUI.h"
#include "Settings.h"

#include "SKSEMenuFramework.h"

#include <string>

namespace {
    bool DrawCommonControls(const char* label, Settings::WidgetConfig& w,
                             float maxIconHeight, bool hasText = true, bool hasOpacity = false) {
        bool dirty = false;
        const std::string s = std::string("##") + label;
        ImGuiMCP::Text("%s", label);
        if (ImGuiMCP::Checkbox(("Enabled" + s).c_str(),            &w.enabled))      dirty = true;
        if (hasText && ImGuiMCP::Checkbox(("Show numeric value" + s).c_str(), &w.showText))     dirty = true;
        if (ImGuiMCP::SliderFloat(("X" + s).c_str(),               &w.x,            0.0f, 3840.0f, "%.0f"))        dirty = true;
        if (ImGuiMCP::SliderFloat(("Y" + s).c_str(),               &w.y,            0.0f, 2160.0f, "%.0f"))        dirty = true;
        if (ImGuiMCP::SliderFloat(("Icon height (px)" + s).c_str(),&w.iconHeightPx, 16.0f, maxIconHeight, "%.0f")) dirty = true;
        if (hasText && ImGuiMCP::SliderFloat(("Text size (px)" + s).c_str(),  &w.textSizePx,   8.0f, 128.0f, "%.0f"))         dirty = true;
        // 0.4.0: opacity 0-100 % (config.json "opacity"; missing = 100).
        if (hasOpacity && ImGuiMCP::SliderFloat(("Opacity (%)" + s).c_str(), &w.opacityPct, 0.0f, 100.0f, "%.0f%%")) dirty = true;
        return dirty;
    }

    // 0.4.0: labels for Config::anWidgets (same index order as Settings::kANKeys).
    constexpr const char* kANLabels[Settings::kANCount] = {
        "Advanced Nudity - ass",
        "Advanced Nudity - boobs",
        "Advanced Nudity - vagina",
    };

    // True while the three AN widgets still sit at their built-in default spots, i.e. the
    // user has never placed them.
    bool ANWidgetsAtDefaults(const Settings::Config& c) {
        const Settings::Config d{};
        for (int i = 0; i < Settings::kANCount; ++i) {
            const auto& a = c.anWidgets[i];
            const auto& b = d.anWidgets[i];
            if (a.x != b.x || a.y != b.y || a.iconHeightPx != b.iconHeightPx) return false;
        }
        return true;
    }

    // First switch to separate widgets: start them in a row next to wherever the arousal
    // icon is NOW (the built-in defaults are only "next to" the default icon position).
    // Same height as the icon, so each piece of overlay art keeps its on-icon scale.
    // Row goes right of the icon, or left if it would run past the 3840 px slider range.
    void SeedANWidgetsNextToIcon(Settings::Config& c) {
        constexpr float kGap = 8.0f;
        const float h    = c.arousal.iconHeightPx;
        const float step = h + kGap;
        const bool  left = c.arousal.x + step * static_cast<float>(Settings::kANCount) + h > 3840.0f;
        for (int i = 0; i < Settings::kANCount; ++i) {
            auto& w = c.anWidgets[i];
            const float x = left ? c.arousal.x - step * (i + 1) : c.arousal.x + step * (i + 1);
            w.x = x < 0.0f ? 0.0f : x;
            w.y = c.arousal.y;
            w.iconHeightPx = h;
        }
    }

    void __stdcall RenderLayout() {
        // Lets the separate AN widgets draw as a placement preview while this page is open.
        HudUI::NoteLayoutPageOpen();

        Settings::Config cfg;
        { auto lk = Settings::Lock(); cfg = Settings::Get(); }
        bool dirty = false;

        dirty |= DrawCommonControls("Arousal", cfg.arousal, /*maxIconHeight*/ 1024.0f,
                                    /*hasText*/ true, /*hasOpacity*/ true);
        if (ImGuiMCP::Checkbox("Glow pulse at max arousal##Arousal", &cfg.arousal.glowPulse)) dirty = true;
        if (ImGuiMCP::Checkbox("Advanced Nudity overlays##Arousal", &cfg.arousal.anOverlays)) dirty = true;
        if (ImGuiMCP::Checkbox("Advanced Nudity overlays as separate widgets##Arousal", &cfg.arousal.anSeparate)) {
            dirty = true;
            if (cfg.arousal.anSeparate && ANWidgetsAtDefaults(cfg)) SeedANWidgetsNextToIcon(cfg);
        }
        if (ImGuiMCP::Checkbox("Shift+crosshair NPC arousal peek##Arousal", &cfg.arousal.npcCrosshair)) dirty = true;

        // 0.4.0: one position/size/opacity block per Advanced Nudity overlay.
        if (cfg.arousal.anOverlays && cfg.arousal.anSeparate) {
            ImGuiMCP::Text("");
            if (HudUI::ANOverlaysAvailable()) {
                ImGuiMCP::Text("All three are shown while this page is open so you can place them;");
                ImGuiMCP::Text("in game each one appears only while that region is flashed.");
                ImGuiMCP::Text("Each has its own Enabled box; the Arousal Enabled box above does not hide them.");
            } else {
                ImGuiMCP::Text("Advanced Nudity Detection.esp or the arousedAN*.dds art was not found - these stay hidden.");
            }
            for (int i = 0; i < Settings::kANCount; ++i) {
                ImGuiMCP::Text("");
                dirty |= DrawCommonControls(kANLabels[i], cfg.anWidgets[i], /*maxIconHeight*/ 1024.0f,
                                            /*hasText*/ false, /*hasOpacity*/ true);
            }
        }

        // 2026-09-12: TDF auto-whoring indicator - its own position/scale, same page.
        ImGuiMCP::Text("");
        dirty |= DrawCommonControls("Prostitution indicator (TDF auto-whoring & Radiant)", cfg.whoring,
                                    /*maxIconHeight*/ 1024.0f, /*hasText*/ false);
        ImGuiMCP::Text(HudUI::WhoringSourceStatus());

        if (dirty) Settings::MarkDirty();
        { auto lk = Settings::Lock(); Settings::Get() = cfg; }
    }

    void __stdcall RenderCadence() {
        Settings::Config cfg;
        { auto lk = Settings::Lock(); cfg = Settings::Get(); }
        bool dirty = false;
        if (ImGuiMCP::SliderInt("Arousal cadence (sec)", &cfg.arousalCadenceSec, 1, 30)) dirty = true;
        if (dirty) Settings::MarkDirty();
        { auto lk = Settings::Lock(); Settings::Get() = cfg; }
    }

    void __stdcall RenderHotkey() {
        Settings::Config cfg;
        { auto lk = Settings::Lock(); cfg = Settings::Get(); }
        bool dirty = false;
        ImGuiMCP::Text("Hide-toggle hotkey (DirectInput scan code)");
        ImGuiMCP::Text("Common: 14=Backspace, 15=Tab, 28=Enter, 57=Space, 0=unbound");
        if (ImGuiMCP::SliderInt("Scan code##hide", &cfg.hideHotkeyDX, 0, 220)) dirty = true;
        ImGuiMCP::Text("");
        ImGuiMCP::Text("Auto-hide rules");
        if (ImGuiMCP::Checkbox("Follow compass hide (iHUD / Sandbox When Idle / etc.)",
                                &cfg.followCompassHide)) dirty = true;
        if (dirty) Settings::MarkDirty();
        { auto lk = Settings::Lock(); Settings::Get() = cfg; }
    }
}

namespace SettingsUI {

    void Register() {
        const bool installed = SKSEMenuFramework::IsInstalled();
        const HMODULE handle = ::GetModuleHandleW(L"SKSEMenuFramework");
        SKSE::log::info("SettingsUI::Register - IsInstalled={} GetModuleHandle={}",
                        installed, static_cast<void*>(handle));
        if (!installed || !handle) {
            SKSE::log::error("SKSE Menu Framework not loadable - settings menu unavailable");
            return;
        }
        SKSEMenuFramework::SetSection("Aroused Widget");
        SKSEMenuFramework::AddSectionItem("Layout",     RenderLayout);
        SKSEMenuFramework::AddSectionItem("Cadence",    RenderCadence);
        SKSEMenuFramework::AddSectionItem("Visibility", RenderHotkey);
        SKSE::log::info("SettingsUI: registered 'Aroused Widget' section in MCP");
    }
}
