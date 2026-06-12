#include "HudUI.h"

#include "ArousalReader.h"
#include "Settings.h"
#include "Visibility.h"

#include "SKSEMenuFramework.h"

#include <array>
#include <cstdio>

namespace {
    std::array<std::array<ImGuiMCP::ImTextureID, 9>, 2> g_arousalTex{};
    bool g_loaded = false;

    constexpr float kArousalNativeW = 100.0f;
    constexpr float kArousalNativeH = 100.0f;
    constexpr float kRefFontSize    = 16.0f;

    constexpr int OverlayFlags =
        ImGuiMCP::ImGuiWindowFlags_NoTitleBar
        | ImGuiMCP::ImGuiWindowFlags_NoResize
        | ImGuiMCP::ImGuiWindowFlags_NoMove
        | ImGuiMCP::ImGuiWindowFlags_NoScrollbar
        | ImGuiMCP::ImGuiWindowFlags_NoCollapse
        | ImGuiMCP::ImGuiWindowFlags_NoBackground
        | ImGuiMCP::ImGuiWindowFlags_NoSavedSettings
        | ImGuiMCP::ImGuiWindowFlags_NoInputs
        | ImGuiMCP::ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiMCP::ImGuiWindowFlags_NoNav
        | ImGuiMCP::ImGuiWindowFlags_NoDocking;

    int LevelFromValue(int v) {
        if (v <= 0) return 0;
        if (v >= 100) return 8;
        return v / 12;
    }

    ImGuiMCP::ImVec2 IconSize(float nativeW, float nativeH, float targetH) {
        if (nativeH <= 0.0f) return { targetH, targetH };
        return { targetH * (nativeW / nativeH), targetH };
    }

    void __stdcall RenderArousal() {
        if (!g_loaded) return;
        if (!Visibility::ShouldRender()) return;
        auto val = ArousalReader::GetArousalCached();
        if (!val) return;
        Settings::ArousalConfig cfg;
        { auto lk = Settings::Lock(); cfg = Settings::Get().arousal; }
        if (!cfg.enabled) return;

        const int set   = (cfg.imageSet == 1) ? 1 : 0;
        const int level = LevelFromValue(*val);
        ImGuiMCP::ImTextureID tex = g_arousalTex[set][level];
        if (!tex) return;

        ImGuiMCP::SetNextWindowPos({ cfg.x, cfg.y },
                                    ImGuiMCP::ImGuiCond_Always, { 0, 0 });
        bool open = true;
        if (ImGuiMCP::Begin("##hudwidget_arousal", &open, OverlayFlags)) {
            ImGuiMCP::Image(tex, IconSize(kArousalNativeW, kArousalNativeH, cfg.iconHeightPx));
            if (cfg.showText) {
                ImGuiMCP::SetWindowFontScale(cfg.textSizePx / kRefFontSize);
                ImGuiMCP::Text("%d%%", *val);
                ImGuiMCP::SetWindowFontScale(1.0f);
            }
        }
        ImGuiMCP::End();
    }
}

namespace HudUI {

    void Register() {
        const bool installed = SKSEMenuFramework::IsInstalled();
        const HMODULE handle = ::GetModuleHandleW(L"SKSEMenuFramework");
        SKSE::log::info("HudUI::Register - IsInstalled={} GetModuleHandle={}",
                        installed, static_cast<void*>(handle));
        if (!installed || !handle) {
            SKSE::log::error("SKSE Menu Framework not loadable - HUD widgets won't render");
            return;
        }

        int loaded = 0;
        for (int i = 0; i < 9; ++i) {
            char p[96];
            // Keep texture path at Interface/HUDWidgets — textures live in the
            // user's separate "SexLab Widgets--Claude" mod folder under that
            // path. Internal-only; not user-visible.
            std::snprintf(p, sizeof(p), "Data/Interface/HUDWidgets/aroused/aroused%d.dds", i);
            g_arousalTex[0][i] = SKSEMenuFramework::LoadTexture(p);
            if (g_arousalTex[0][i]) ++loaded;
            std::snprintf(p, sizeof(p), "Data/Interface/HUDWidgets/exposure/exp%d.dds", i);
            g_arousalTex[1][i] = SKSEMenuFramework::LoadTexture(p);
            if (g_arousalTex[1][i]) ++loaded;
        }
        SKSE::log::info("HudUI::Register - loaded {}/18 textures", loaded);
        g_loaded = true;

        SKSEMenuFramework::AddHudElement(RenderArousal);
        SKSE::log::info("HudUI::Register - 1 HUD element registered");
    }
}
