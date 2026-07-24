#include "HudUI.h"

#include "ArousalReader.h"
#include "Settings.h"
#include "Visibility.h"

#include "SKSEMenuFramework.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

#include <Windows.h>   // GetAsyncKeyState (Shift held)

namespace {
    std::array<std::array<ImGuiMCP::ImTextureID, 9>, 2> g_arousalTex{};
    bool g_loaded = false;

    // --Claude 2026-07-24: max-arousal glow stage (aroused9.dds) — crossfaded over
    // aroused8 while arousal is maxed. 2s up / 2s down (user spec: "about 2 seconds each").
    ImGuiMCP::ImTextureID g_glowTex = nullptr;

    // --Claude 2026-07-24: Advanced Nudity flash overlays. AND tracks the states as
    // faction ranks on the player (Advanced Nudity Detection.esp): ShowAss 0x82E,
    // ShowChest 0x82F, ShowGenitals 0x830 — rank >= 1 means that region is flashed.
    // (Same faction map PEM's DAV->AND sync uses.)
    constexpr const char* kANEsp = "Advanced Nudity Detection.esp";
    struct ANOverlay {
        RE::FormID            id;
        const char*           dds;
        RE::TESFaction*       faction = nullptr;
        ImGuiMCP::ImTextureID tex     = nullptr;
        bool                  active  = false;   // cached rank check
    };
    std::array<ANOverlay, 3> g_an{ {
        { 0x00082E, "arousedANass" },
        { 0x00082F, "arousedANboobs" },
        { 0x000830, "arousedANvagina" },
    } };
    bool g_anAvailable = false;

    // Re-read the AND faction ranks at most twice a second — cheap, and render
    // stays a cached lookup.
    void RefreshANStates() {
        using clock = std::chrono::steady_clock;
        static clock::time_point s_next{};
        const auto now = clock::now();
        if (now < s_next) return;
        s_next = now + std::chrono::milliseconds(500);

        auto* pc = RE::PlayerCharacter::GetSingleton();
        if (!pc) return;
        for (auto& o : g_an) {
            o.active = o.faction && o.tex && pc->GetFactionRank(o.faction, true) >= 1;
        }
    }

    // 0..1 glow weight: cosine ping-pong, 2s rise + 2s fall.
    float GlowAlpha() {
        using clock = std::chrono::steady_clock;
        static const clock::time_point s_start = clock::now();
        const float t = std::chrono::duration<float>(clock::now() - s_start).count();
        return 0.5f * (1.0f - std::cos(3.14159265f * t / 2.0f));
    }

    // --Claude 2026-07-25: Shift + crosshair NPC (out of combat / no menu) peeks
    // that NPC's arousal on the widget for 5s, then reverts to the player. NPC
    // arousal comes from the same OSLArousedNative.GetArousalNoSideEffects native
    // (works on any actor), read async via ArousalReader::ReadActorArousal.
    std::chrono::steady_clock::time_point g_npcShowUntil{};
    std::string                           g_npcName;

    RE::Actor* CrosshairNpc() {
        auto* cd = RE::CrosshairPickData::GetSingleton();
        if (!cd) return nullptr;
        RE::NiPointer<RE::TESObjectREFR> refr = cd->targetActor.get();
        if (!refr) refr = cd->target.get();
        if (!refr) return nullptr;
        auto* a = skyrim_cast<RE::Actor*>(refr.get());
        if (!a || a->IsDead()) return nullptr;
        if (a == static_cast<RE::Actor*>(RE::PlayerCharacter::GetSingleton())) return nullptr;
        return a;
    }

    // Called each frame; while Shift is held over a valid NPC, (re)start the 5s
    // window and fire an async read of that NPC's arousal.
    void NpcProbe() {
        if ((::GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0) return;
        auto* pc = RE::PlayerCharacter::GetSingleton();
        if (!pc || pc->IsInCombat()) return;
        if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) return;
        RE::Actor* npc = CrosshairNpc();
        if (!npc) return;
        ArousalReader::ReadActorArousal(npc);
        if (const char* n = npc->GetDisplayFullName()) g_npcName = n;
        g_npcShowUntil = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    }

    bool NpcShowActive() { return std::chrono::steady_clock::now() < g_npcShowUntil; }

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
        Settings::ArousalConfig cfg;
        { auto lk = Settings::Lock(); cfg = Settings::Get().arousal; }
        if (!cfg.enabled) return;

        // --Claude: Shift + crosshair NPC → show that NPC's arousal for 5s.
        bool showNpc = false;
        int  displayVal = 0;
        if (cfg.npcCrosshair) {
            NpcProbe();
            if (NpcShowActive()) {
                if (auto nv = ArousalReader::GetNpcArousalCached()) { displayVal = *nv; showNpc = true; }
            }
        }
        if (!showNpc) {
            auto val = ArousalReader::GetArousalCached();
            if (!val) return;
            displayVal = *val;
        }

        const int set   = (cfg.imageSet == 1) ? 1 : 0;
        const int level = LevelFromValue(displayVal);
        ImGuiMCP::ImTextureID tex = g_arousalTex[set][level];
        if (!tex) return;

        ImGuiMCP::SetNextWindowPos({ cfg.x, cfg.y },
                                    ImGuiMCP::ImGuiCond_Always, { 0, 0 });
        bool open = true;
        if (ImGuiMCP::Begin("##hudwidget_arousal", &open, OverlayFlags)) {
            const auto size    = IconSize(kArousalNativeW, kArousalNativeH, cfg.iconHeightPx);
            const auto basePos = ImGuiMCP::GetCursorPos();

            // Base stage icon. At max arousal (level 8, aroused set) the glow stage
            // pulses on top: aroused9 fully covers aroused8 at weight 1, so layering
            // reads as a clean crossfade with no mid-fade translucency dip.
            ImGuiMCP::Image(tex, size);
            if (cfg.glowPulse && set == 0 && level == 8 && g_glowTex) {
                ImGuiMCP::SetCursorPos(basePos);
                ImGuiMCP::Image(g_glowTex, size, { 0, 0 }, { 1, 1 },
                                { 1.0f, 1.0f, 1.0f, GlowAlpha() });
            }

            // Advanced Nudity flash overlays — one layer per flashed region.
            // Player-only (they read the player's factions); skip while peeking an NPC.
            if (!showNpc && cfg.anOverlays && g_anAvailable) {
                RefreshANStates();
                for (const auto& o : g_an) {
                    if (!o.active) continue;
                    ImGuiMCP::SetCursorPos(basePos);
                    ImGuiMCP::Image(o.tex, size);
                }
            }

            if (cfg.showText) {
                ImGuiMCP::SetWindowFontScale(cfg.textSizePx / kRefFontSize);
                ImGuiMCP::Text("%d%%", displayVal);
                // When peeking an NPC, label whose arousal this is.
                if (showNpc && !g_npcName.empty()) {
                    ImGuiMCP::Text("%s", g_npcName.c_str());
                }
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

        // --Claude: max-arousal glow stage (optional — absent file just disables the pulse)
        g_glowTex = SKSEMenuFramework::LoadTexture("Data/Interface/HUDWidgets/aroused/aroused9.dds");
        SKSE::log::info("HudUI::Register - glow stage (aroused9) {}", g_glowTex ? "loaded" : "not found");

        // --Claude: Advanced Nudity overlays — resolve textures + AND factions (we run
        // at kDataLoaded, so form lookup is safe). Missing mod or files = feature off.
        auto* dh = RE::TESDataHandler::GetSingleton();
        int anReady = 0;
        for (auto& o : g_an) {
            char p[96];
            std::snprintf(p, sizeof(p), "Data/Interface/HUDWidgets/aroused/%s.dds", o.dds);
            o.tex     = SKSEMenuFramework::LoadTexture(p);
            o.faction = dh ? dh->LookupForm<RE::TESFaction>(o.id, kANEsp) : nullptr;
            if (o.tex && o.faction) ++anReady;
        }
        g_anAvailable = anReady > 0;
        SKSE::log::info("HudUI::Register - AND overlays ready: {}/3 (esp {})",
                        anReady, g_anAvailable ? "found" : "missing/none");

        g_loaded = true;

        SKSEMenuFramework::AddHudElement(RenderArousal);
        SKSE::log::info("HudUI::Register - 1 HUD element registered");
    }
}
