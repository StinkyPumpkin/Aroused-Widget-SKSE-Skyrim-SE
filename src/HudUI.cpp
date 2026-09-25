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
    // v0.3.2: exposure image set removed — no exposure art ships, and picking it
    // left the widget invisible (null textures). Single aroused set only.
    std::array<ImGuiMCP::ImTextureID, 9> g_arousalTex{};
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
        const char*           window;                // 0.4.0: ImGui id when drawn as its own widget
        RE::TESFaction*       faction = nullptr;
        ImGuiMCP::ImTextureID tex     = nullptr;
        bool                  active  = false;   // cached rank check
    };
    // Index order = Settings::ANRegion (0 ass, 1 boobs, 2 vagina) = Config::anWidgets.
    std::array<ANOverlay, Settings::kANCount> g_an{ {
        { 0x00082E, "arousedANass",    "##hudwidget_anAss" },
        { 0x00082F, "arousedANboobs",  "##hudwidget_anBoobs" },
        { 0x000830, "arousedANvagina", "##hudwidget_anVagina" },
    } };
    bool g_anAvailable = false;

    // 0.4.0: the separate AN widgets only draw while their region is flashed, which would
    // make them impossible to place. SettingsUI stamps this every frame the Layout page is
    // drawn; while it is fresh, all enabled AN widgets draw as a placement preview. Both
    // callers run on SKSE Menu Framework's render thread (HUD elements, then windows).
    std::chrono::steady_clock::time_point g_layoutPageSeen{};

    bool LayoutPageOpen() {
        return std::chrono::steady_clock::now() - g_layoutPageSeen < std::chrono::milliseconds(250);
    }

    float OpacityOf(const Settings::WidgetConfig& a_cfg) {
        const float a = a_cfg.opacityPct / 100.0f;
        return a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
    }

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

    // --Claude 2026-07-25: TAP-TO-LATCH NPC arousal peek. Shift + crosshair on an
    // NPC latches to it and shows its arousal on the widget; it keeps showing
    // (Shift can be released) until you LOOK AWAY (crosshair leaves that NPC),
    // then reverts to the player. NPC arousal = OSLArousedNative.GetArousalNoSideEffects
    // (works on any actor), read async + throttled via ArousalReader::ReadActorArousal.
    RE::FormID                            g_latchNpc = 0;
    std::string                           g_npcName;
    std::chrono::steady_clock::time_point g_npcNextRead{};

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

    void NpcProbe() {
        RE::Actor* cross = nullptr;
        auto* pc = RE::PlayerCharacter::GetSingleton();
        auto* ui = RE::UI::GetSingleton();
        const bool gameplay = pc && !pc->IsInCombat() && !(ui && ui->GameIsPaused());
        if (gameplay) cross = CrosshairNpc();

        // Shift + crosshair NPC -> latch to it.
        if (gameplay && cross && (::GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
            g_latchNpc = cross->GetFormID();
        }

        if (g_latchNpc) {
            // Still looking at the latched NPC? keep showing + refresh (throttled).
            if (cross && cross->GetFormID() == g_latchNpc) {
                const auto now = std::chrono::steady_clock::now();
                if (now >= g_npcNextRead) {
                    ArousalReader::ReadActorArousal(cross);
                    if (const char* n = cross->GetDisplayFullName()) g_npcName = n;
                    g_npcNextRead = now + std::chrono::milliseconds(400);
                }
            } else {
                g_latchNpc = 0;   // looked away -> revert to player
            }
        }
    }

    bool NpcShowActive() { return g_latchNpc != 0; }

    // --Claude 2026-09-12: TDF Enhanced Prostitution auto-whoring indicator.
    // State = GlobalVariable BB_PlayerAutoWhoreEnabled (0x000BF7 in TDF Enhanced
    // Prostitution.esp), 1 while the hotkey/MCM has auto-whoring on. Resolved once at
    // kDataLoaded; the render reads the global's float directly (plain field read, same
    // class of access as the AND faction ranks above). Missing esp/texture = feature off.
    constexpr const char* kTDFEsp      = "TDF Enhanced Prostitution.esp";
    constexpr RE::FormID  kTDFAutoWhore = 0x000BF7;
    RE::TESGlobal*        g_whoringGlobal = nullptr;
    ImGuiMCP::ImTextureID g_whoringTex    = nullptr;
    const char*           g_whoringStatus = "TDF Enhanced Prostitution: not checked yet";
    constexpr float kWhoringNativeW = 100.0f;
    constexpr float kWhoringNativeH = 100.0f;

    // --Claude 2026-09-13: Radiant Prostitution also has a mode where NPCs walk up to
    // the player, so "someone is approaching" on its own does not say which mod is
    // driving it. Both are now detected and the icon says which: normal = TDF,
    // mirrored left-to-right = Radiant.
    //
    // TDF's signal is the player's own toggle (the global above). Radiant has no
    // equivalent global - its passive/active mode lives in Papyrus script properties on
    // mf_Prostitute_Handler (mf_Variables.PassiveSolicit), which is awkward to read from
    // native code. The reliable native signal is its client alias being FILLED: that only
    // happens once RP has actually picked someone to send over, which is the moment the
    // widget wants to report anyway.
    //
    //   mf_SolicitePlayer       0x011136  alias 5 "theClient"  (runs the approach roll)
    //   mf_SolicitePlayerPooler 0x02AB81  alias 1 "aClient"    (candidate pooler)
    //
    // For reference, TDF's side of the same mechanism is BB_PlayerAutoWhoringPooler
    // (0x000BDC), whose pNearbyActor1..10 aliases carry BB_PlayerAutoWhoringQuestPackage
    // (0x000BEE, template ForceGreet) - that package is what walks an NPC over.
    constexpr const char*   kRPEsp                 = "MF_RadiantProstitution.esp";
    constexpr RE::FormID    kRPSolicitPlayer       = 0x011136;
    constexpr RE::FormID    kRPSolicitPooler       = 0x02AB81;
    constexpr std::uint32_t kRPSolicitPlayerClient = 5;
    constexpr std::uint32_t kRPSolicitPoolerClient = 1;
    RE::TESQuest*           g_rpSolicitPlayer = nullptr;
    RE::TESQuest*           g_rpSolicitPooler = nullptr;

    enum class WhoreSource { kNone, kTDF, kRadiant };

    // True when the quest is running AND the given alias currently holds a reference.
    bool QuestAliasFilled(RE::TESQuest* a_quest, std::uint32_t a_aliasID) {
        if (!a_quest || !a_quest->IsRunning()) return false;
        RE::BSReadLockGuard lk(a_quest->aliasAccessLock);
        const auto it = a_quest->refAliasMap.find(a_aliasID);
        return it != a_quest->refAliasMap.end() && it->second.get() != nullptr;
    }

    // TDF wins a tie: its global is an explicit player toggle, RP's is incidental state.
    WhoreSource CurrentWhoreSource() {
        if (g_whoringGlobal && g_whoringGlobal->value >= 0.5f) return WhoreSource::kTDF;
        if (QuestAliasFilled(g_rpSolicitPlayer, kRPSolicitPlayerClient) ||
            QuestAliasFilled(g_rpSolicitPooler, kRPSolicitPoolerClient)) {
            return WhoreSource::kRadiant;
        }
        return WhoreSource::kNone;
    }

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
        | ImGuiMCP::ImGuiWindowFlags_NoDocking
        // Without this the window keeps its first-frame size (fitted to the default
        // 64px icon) and larger icon settings get clipped at the window edge.
        | ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize;

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

        const int level = LevelFromValue(displayVal);
        ImGuiMCP::ImTextureID tex = g_arousalTex[level];
        if (!tex) return;

        // 0.4.0: opacity slider. style.Alpha multiplies every colour ImGui emits inside the
        // window - Image tints (icon, glow, AN overlays) and Text alike (ImGui 1.90.8
        // GetColorU32) - so one push covers the whole widget.
        const float alpha = OpacityOf(cfg);
        if (alpha <= 0.0f) return;

        ImGuiMCP::SetNextWindowPos({ cfg.x, cfg.y },
                                    ImGuiMCP::ImGuiCond_Always, { 0, 0 });
        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_Alpha, alpha);
        bool open = true;
        if (ImGuiMCP::Begin("##hudwidget_arousal", &open, OverlayFlags)) {
            const auto size    = IconSize(kArousalNativeW, kArousalNativeH, cfg.iconHeightPx);
            const auto basePos = ImGuiMCP::GetCursorPos();

            // Base stage icon. At max arousal (level 8, aroused set) the glow stage
            // pulses on top: aroused9 fully covers aroused8 at weight 1, so layering
            // reads as a clean crossfade with no mid-fade translucency dip.
            ImGuiMCP::Image(tex, size);
            if (cfg.glowPulse && level == 8 && g_glowTex) {
                ImGuiMCP::SetCursorPos(basePos);
                ImGuiMCP::Image(g_glowTex, size, { 0, 0 }, { 1, 1 },
                                { 1.0f, 1.0f, 1.0f, GlowAlpha() });
            }

            // Advanced Nudity flash overlays — one layer per flashed region.
            // Player-only (they read the player's factions); skip while peeking an NPC.
            // 0.4.0: with "separate widgets" on they are drawn by RenderANSeparate instead.
            if (!showNpc && cfg.anOverlays && !cfg.anSeparate && g_anAvailable) {
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
        ImGuiMCP::PopStyleVar();
    }

    // 0.4.0 (Nexus request): the three Advanced Nudity overlays as separate widgets, each
    // with its own position / size / opacity (Config::anWidgets). Same art, same player
    // faction-rank state as the on-icon overlays; only the placement differs.
    void __stdcall RenderANSeparate() {
        if (!g_loaded || !g_anAvailable) return;
        Settings::ArousalConfig arousal;
        std::array<Settings::WidgetConfig, Settings::kANCount> widgets;
        {
            auto lk = Settings::Lock();
            arousal = Settings::Get().arousal;
            widgets = Settings::Get().anWidgets;
        }
        if (!arousal.anOverlays || !arousal.anSeparate) return;
        if (!Visibility::ShouldRender()) return;

        RefreshANStates();
        const bool preview = LayoutPageOpen();
        for (std::size_t i = 0; i < g_an.size(); ++i) {
            const auto& o = g_an[i];
            const auto& w = widgets[i];
            if (!o.tex || !o.faction || !w.enabled) continue;
            if (!o.active && !preview) continue;
            const float alpha = OpacityOf(w);
            if (alpha <= 0.0f) continue;

            ImGuiMCP::SetNextWindowPos({ w.x, w.y }, ImGuiMCP::ImGuiCond_Always, { 0, 0 });
            ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_Alpha, alpha);
            bool open = true;
            if (ImGuiMCP::Begin(o.window, &open, OverlayFlags)) {
                ImGuiMCP::Image(o.tex, IconSize(kArousalNativeW, kArousalNativeH, w.iconHeightPx));
            }
            ImGuiMCP::End();
            ImGuiMCP::PopStyleVar();
        }
    }

    void __stdcall RenderWhoring() {
        if (!g_loaded || !g_whoringTex) return;
        if (!Visibility::ShouldRender()) return;
        Settings::WhoringConfig cfg;
        { auto lk = Settings::Lock(); cfg = Settings::Get().whoring; }
        if (!cfg.enabled) return;

        const WhoreSource src = CurrentWhoreSource();
        if (src == WhoreSource::kNone) return;   // nobody is soliciting -> draw nothing

        // Mirrored left-to-right marks a Radiant Prostitution approach; TDF draws normal.
        // (Swap the two pairs below for a top-to-bottom flip instead.)
        const bool mirror = (src == WhoreSource::kRadiant);
        const ImGuiMCP::ImVec2 uv0 = mirror ? ImGuiMCP::ImVec2{ 1, 0 } : ImGuiMCP::ImVec2{ 0, 0 };
        const ImGuiMCP::ImVec2 uv1 = mirror ? ImGuiMCP::ImVec2{ 0, 1 } : ImGuiMCP::ImVec2{ 1, 1 };

        ImGuiMCP::SetNextWindowPos({ cfg.x, cfg.y }, ImGuiMCP::ImGuiCond_Always, { 0, 0 });
        bool open = true;
        if (ImGuiMCP::Begin("##hudwidget_whoring", &open, OverlayFlags)) {
            ImGuiMCP::Image(g_whoringTex, IconSize(kWhoringNativeW, kWhoringNativeH, cfg.iconHeightPx),
                            uv0, uv1);
        }
        ImGuiMCP::End();
    }
}

namespace HudUI {

    const char* WhoringSourceStatus() { return g_whoringStatus; }

    bool ANOverlaysAvailable() { return g_anAvailable; }

    void NoteLayoutPageOpen() { g_layoutPageSeen = std::chrono::steady_clock::now(); }

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
            g_arousalTex[i] = SKSEMenuFramework::LoadTexture(p);
            if (g_arousalTex[i]) ++loaded;
        }
        SKSE::log::info("HudUI::Register - loaded {}/9 textures", loaded);

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

        // --Claude 2026-09-12: auto-whoring indicator source + art.
        g_whoringTex    = SKSEMenuFramework::LoadTexture("Data/Interface/HUDWidgets/aroused/whoring.dds");
        g_whoringGlobal = dh ? dh->LookupForm<RE::TESGlobal>(kTDFAutoWhore, kTDFEsp) : nullptr;
        // --Claude 2026-09-13: Radiant Prostitution's solicit quests, for the mirrored icon.
        g_rpSolicitPlayer = dh ? dh->LookupForm<RE::TESQuest>(kRPSolicitPlayer, kRPEsp) : nullptr;
        g_rpSolicitPooler = dh ? dh->LookupForm<RE::TESQuest>(kRPSolicitPooler, kRPEsp) : nullptr;
        const bool rpFound = (g_rpSolicitPlayer || g_rpSolicitPooler);

        if (!g_whoringTex) {
            g_whoringStatus = "whoring.dds missing from Interface/HUDWidgets/aroused - indicator inactive";
        } else if (g_whoringGlobal && rpFound) {
            g_whoringStatus = "TDF + Radiant Prostitution found - normal icon = TDF, mirrored icon = Radiant";
        } else if (g_whoringGlobal) {
            g_whoringStatus = "TDF Enhanced Prostitution.esp found (Radiant not installed) - indicator active while auto-whoring is on";
        } else if (rpFound) {
            g_whoringStatus = "Radiant Prostitution found (TDF not installed) - mirrored icon while a Radiant client is approaching";
        } else {
            g_whoringStatus = "Neither TDF Enhanced Prostitution nor Radiant Prostitution installed - indicator inactive";
        }
        SKSE::log::info("HudUI::Register - whoring indicator: TDF global {} / RP solicit {} + pooler {} / texture {}",
                        g_whoringGlobal ? "found" : "missing",
                        g_rpSolicitPlayer ? "found" : "missing",
                        g_rpSolicitPooler ? "found" : "missing",
                        g_whoringTex ? "loaded" : "missing");

        g_loaded = true;

        SKSEMenuFramework::AddHudElement(RenderArousal);
        SKSEMenuFramework::AddHudElement(RenderANSeparate);
        SKSEMenuFramework::AddHudElement(RenderWhoring);
        SKSE::log::info("HudUI::Register - 3 HUD elements registered");
    }
}
