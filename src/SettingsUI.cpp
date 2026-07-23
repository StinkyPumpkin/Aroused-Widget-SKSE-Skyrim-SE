#include "SettingsUI.h"
#include "Settings.h"

#include "SKSEMenuFramework.h"

#include <string>

namespace {
    bool DrawCommonControls(const char* label, Settings::WidgetConfig& w,
                             float maxIconHeight) {
        bool dirty = false;
        const std::string s = std::string("##") + label;
        ImGuiMCP::Text("%s", label);
        if (ImGuiMCP::Checkbox(("Enabled" + s).c_str(),            &w.enabled))      dirty = true;
        if (ImGuiMCP::Checkbox(("Show numeric value" + s).c_str(), &w.showText))     dirty = true;
        if (ImGuiMCP::SliderFloat(("X" + s).c_str(),               &w.x,            0.0f, 3840.0f, "%.0f"))        dirty = true;
        if (ImGuiMCP::SliderFloat(("Y" + s).c_str(),               &w.y,            0.0f, 2160.0f, "%.0f"))        dirty = true;
        if (ImGuiMCP::SliderFloat(("Icon height (px)" + s).c_str(),&w.iconHeightPx, 16.0f, maxIconHeight, "%.0f")) dirty = true;
        if (ImGuiMCP::SliderFloat(("Text size (px)" + s).c_str(),  &w.textSizePx,   8.0f, 64.0f, "%.0f"))          dirty = true;
        return dirty;
    }

    void __stdcall RenderLayout() {
        Settings::Config cfg;
        { auto lk = Settings::Lock(); cfg = Settings::Get(); }
        bool dirty = false;

        dirty |= DrawCommonControls("Arousal", cfg.arousal, /*maxIconHeight*/ 256.0f);
        const char* sets[] = { "Aroused (heart)", "Exposure (drop)" };
        if (ImGuiMCP::Combo("Image set##Arousal", &cfg.arousal.imageSet, sets, 2)) dirty = true;
        if (ImGuiMCP::Checkbox("Glow pulse at max arousal##Arousal", &cfg.arousal.glowPulse)) dirty = true;
        if (ImGuiMCP::Checkbox("Advanced Nudity overlays##Arousal", &cfg.arousal.anOverlays)) dirty = true;

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
