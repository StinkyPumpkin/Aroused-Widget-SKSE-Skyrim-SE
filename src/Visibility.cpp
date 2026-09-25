#include "Visibility.h"
#include "Settings.h"
#include "iHUDBridge.h"

#include <RE/G/GFxMovieView.h>
#include <RE/G/GFxValue.h>
#include <RE/P/PlayerCamera.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/U/UI.h>

#include <atomic>

namespace {
    std::atomic<bool> g_manuallyHidden{false};

    // Paths to MovieClip objects (NOT the _alpha property). We resolve the
    // object via GetVariable, then read alpha/visible via GetDisplayInfo.
    static constexpr const char* kCompassPaths[] = {
        "_root.HUDMovieBaseInstance.CompassShoutMeterHolder",
        "_root.HUDMovieBaseInstance.Compass",
        "_root.HUDMovieBaseInstance.HUDComponent_Compass",
        "_root.HUDMovieBaseInstance.SkyUI_HUDComponent_Compass",
        "_root.HUDMovieBaseInstance.compass_mc",
        "_root.CompassShoutMeterHolder",
        "_root.Compass",
        "HUDMovieBaseInstance.CompassShoutMeterHolder",
        "_root.HUDMovieBaseInstance",
    };

    int  g_resolvedPathIdx  = -1;
    bool g_lastCompassHidden = false;
    bool g_didFirstDump     = false;

    struct CompassRead {
        bool   hidden    = false;
        double alpha     = -1.0;
        bool   visible   = true;
        int    pathIdx   = -1;
    };

    // Try one path. Returns true if the variable resolved AND was a DisplayObject
    // we could call GetDisplayInfo on; out filled with alpha + visible.
    bool TryPath(RE::GFxMovieView* view, const char* path,
                  double& alphaOut, bool& visibleOut) {
        RE::GFxValue obj;
        if (!view->GetVariable(&obj, path)) return false;
        if (!obj.IsObject() && !obj.IsDisplayObject()) {
            // The path may have pointed to a number directly (the old _alpha
            // suffix). Accept that as a degraded read.
            if (obj.IsNumber()) {
                alphaOut   = obj.GetNumber();
                visibleOut = (alphaOut > 0.0);
                return true;
            }
            return false;
        }
        RE::GFxValue::DisplayInfo info;
        if (!obj.GetDisplayInfo(&info)) {
            // No display info but we DID resolve the variable — still partial success
            alphaOut   = -1.0;
            visibleOut = true;
            return false;
        }
        alphaOut   = info.GetAlpha();
        visibleOut = info.GetVisible();
        return true;
    }

    CompassRead ReadCompassAlpha() {
        CompassRead r;
        auto* ui = RE::UI::GetSingleton();
        if (!ui) return r;
        // CRITICAL: Skyrim's HUD menu is registered as "HUD Menu" (space).
        // Constant comes from RE::HUDMenu::MENU_NAME.
        auto view = ui->GetMovieView("HUD Menu");
        if (!view) {
            static bool s_warned = false;
            if (!s_warned) {
                s_warned = true;
                SKSE::log::warn("Visibility: GetMovieView(\"HUD Menu\") returned null");
            }
            return r;
        }

        // First-call dump: try EVERY path, log each result. Helps us find the
        // right path on this user's particular HUDMenu / SkyUI / Edge UI build.
        if (!g_didFirstDump) {
            g_didFirstDump = true;
            SKSE::log::info("Visibility: ===== first-time compass path dump =====");
            for (int i = 0; i < static_cast<int>(std::size(kCompassPaths)); ++i) {
                double a = -1.0;
                bool   v = true;
                bool ok  = TryPath(view.get(), kCompassPaths[i], a, v);
                SKSE::log::info("  [{}] {} -> resolved={} alpha={:.1f} visible={}",
                                i, kCompassPaths[i], ok, a, v);
            }
            SKSE::log::info("Visibility: ===== end dump =====");
        }

        for (int i = 0; i < static_cast<int>(std::size(kCompassPaths)); ++i) {
            double a = -1.0;
            bool   v = true;
            if (TryPath(view.get(), kCompassPaths[i], a, v)) {
                r.alpha   = a;
                r.visible = v;
                r.hidden  = (!v) || (a >= 0.0 && a < 5.0);
                r.pathIdx = i;
                return r;
            }
        }
        return r;
    }

    // v0.3.7: the compass read used to run inside ShouldRender(), i.e. inside SKSE Menu
    // Framework's render callback (the D3D Present hook). GetVariable() walks the HUD
    // movie's ActionScript objects and AddRefs whatever it finds; around load transitions
    // the game's main thread is tearing down / rebuilding that movie while Present keeps
    // firing, so the AddRef landed on a freed object (Nexus report: SkyrimSE.exe+10EA1D9
    // `inc [rax+0x20]` under ArousedWidget -> HudManager::Render, strings "HUD Menu" /
    // "_root.HUDMovieBaseInstance.CompassShoutMeterHolder"). The menu/Is3DLoaded guards a
    // few instructions earlier can't close that window. Now the read runs ONLY on the
    // main thread (SKSE task, queued by WidgetController every 250 ms) and the render
    // path reads these atomics.
    std::atomic<bool> g_compassHidden{false};
    std::atomic<bool> g_compassResolved{false};
    std::atomic<bool> g_pollQueued{false};

    // Verified against CommonLibSSE-NG MENU_NAME constants. Note: spaces
    // and capitalisation are wildly inconsistent in Bethesda's naming.
    constexpr const char* kBlockMenus[] = {
        // v0.3.3: Fader/Mist/LoadWaitSpinner cover the load-adjacent transitions the
        // Loading Menu check alone misses (field report: widget visible during loads).
        "Fader Menu",       "Mist Menu",
        "LoadWaitSpinner",
        "Main Menu",        "Loading Menu",
        "Console",          "MessageBoxMenu",
        "Crafting Menu",    "BarterMenu",
        "ContainerMenu",    "GiftMenu",
        "InventoryMenu",    "MagicMenu",
        "MapMenu",          "FavoritesMenu",
        "Dialogue Menu",    "Journal Menu",
        "Book Menu",        "TweenMenu",
        "Tutorial Menu",    "RaceSex Menu",
        "Sleep/Wait Menu",
    };

    // --Claude 2026-09-26 (0.4.0): WHY the widget is hidden right now. "Disappears
    // randomly" reports could not be attributed: eight different gates can hide the
    // widget and only the compass one ever logged. ShouldRender (render thread) records
    // the gate that fired; WidgetController's thread logs it on change
    // (LogHideReasonChange), so there is no file I/O inside the Present hook.
    // Packed as (reason << 8) | detail in one atomic so a reader never sees a torn pair.
    enum HideReason : int {
        kShown = 0,
        kManual,        // hide hotkey
        kExternal,      // iHUDClaude / TFCam HideAll (detail = iHUDBridge::Hider)
        kPaused,        // UI::GameIsPaused
        kShowMenusOff,  // `tm` / Photo Mode Hide UI
        kNo3D,          // player 3D unloaded (load screens)
        kMenu,          // blocking menu open (detail = kBlockMenus index)
        kAutoVanity,    // auto-vanity (idle) camera
        kCompass,       // following the compass hide
    };
    std::atomic<int> g_hideState{ -1 };   // -1 = ShouldRender has not run yet

    bool Hide(int a_reason, int a_detail = 0) {
        g_hideState.store((a_reason << 8) | (a_detail & 0xFF), std::memory_order_relaxed);
        return false;
    }

    void LogCompassChange(const CompassRead& cur) {
        if (cur.pathIdx != g_resolvedPathIdx) {
            if (cur.pathIdx >= 0) {
                SKSE::log::info("Visibility: compass path resolved -> [{}] '{}'",
                                cur.pathIdx, kCompassPaths[cur.pathIdx]);
            } else {
                SKSE::log::warn("Visibility: NO compass path resolved this tick");
            }
            g_resolvedPathIdx = cur.pathIdx;
        }
        if (cur.hidden != g_lastCompassHidden) {
            SKSE::log::info("Visibility: compass {} (alpha={:.1f} visible={})",
                            cur.hidden ? "hidden" : "visible", cur.alpha, cur.visible);
            g_lastCompassHidden = cur.hidden;
        }
    }
}

namespace Visibility {

    void ToggleManualHide() {
        g_manuallyHidden.store(!g_manuallyHidden.load(), std::memory_order_relaxed);
    }
    bool IsManuallyHidden() {
        return g_manuallyHidden.load(std::memory_order_relaxed);
    }

    bool ShouldRender() {
        // One-shot: log every reason we'd return early on the first call,
        // plus the actual followCompassHide value seen.
        static bool s_dumpedFlow = false;
        if (!s_dumpedFlow) {
            s_dumpedFlow = true;
            bool follow0 = true;
            { auto lk = Settings::Lock(); follow0 = Settings::Get().followCompassHide; }
            SKSE::log::info("Visibility::ShouldRender FIRST CALL - manualHidden={} followCompassHide={}",
                            g_manuallyHidden.load(), follow0);
        }

        if (g_manuallyHidden.load(std::memory_order_relaxed)) return Hide(kManual);

        // External hide via iHUDClaude's "Universal Hide" (Smart or Nuclear) or TFCam's
        // free-camera HUD hide. Modulated by RespectArousalThreshold if iHUDClaude sent one.
        if (const auto hider = iHUDBridge::HiddenBy(); hider != iHUDBridge::Hider::kNone) {
            return Hide(kExternal, static_cast<int>(hider));
        }

        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            g_hideState.store(kShown << 8, std::memory_order_relaxed);
            return true;
        }

        if (ui->GameIsPaused()) return Hide(kPaused);

        // v0.3.5 (Nexus request): follow the game's own "show menus" flag. This is what
        // the `tm` console command and po3's Photo Mode "Hide UI" flip (UI::ShowMenus);
        // it hides every Scaleform menu but not an ImGui overlay, so mirror it here.
        // VR has no Photo Mode and its UI runtime data differs; keep the gate SE/AE-only.
        if (!REL::Module::IsVR() && !ui->IsShowingMenus()) return Hide(kShowMenusOff);

        // Load screens: the menu-name/pause checks below can miss transitions
        // (field report: widget visible during a loading screen). The player's
        // 3D is unloaded during every load — a reliable catch-all.
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) return Hide(kNo3D);

        for (int i = 0; i < static_cast<int>(std::size(kBlockMenus)); ++i) {
            if (ui->IsMenuOpen(kBlockMenus[i])) return Hide(kMenu, i);
        }

        auto* pc = RE::PlayerCamera::GetSingleton();
        if (pc) {
            const auto idx = static_cast<size_t>(RE::CameraStates::kAutoVanity);
            const auto& vanityState = pc->cameraStates[idx];
            const auto& cur = pc->currentState;
            if (cur && vanityState && cur.get() == vanityState.get()) return Hide(kAutoVanity);
        }

        bool follow = true;
        { auto lk = Settings::Lock(); follow = Settings::Get().followCompassHide; }
        if (follow) {
            // Main-thread poll result only - never touch Scaleform from the render path.
            //
            // --Claude 2026-09-15: the arousal threshold has to win here too. Reported
            // as "the widget was pulsing at max, then vanished when the compass auto-hid;
            // iHUD key off/on brings it back". It was this branch: iHUD's Smart Hide
            // hides the compass, we follow it, and RespectArousalThreshold never got a
            // say because it was only consulted in IsHiddenByExternal() above. If the
            // user asked us to stay up at high arousal, that must hold no matter WHICH
            // path wants the widget gone.
            if (g_compassResolved.load(std::memory_order_relaxed) &&
                g_compassHidden.load(std::memory_order_relaxed) &&
                !iHUDBridge::ArousalAboveRespectThreshold()) return Hide(kCompass);
        }

        g_hideState.store(kShown << 8, std::memory_order_relaxed);
        return true;
    }

    void LogHideReasonChange() {
        static int s_logged = -1;   // WidgetController thread only
        const int cur = g_hideState.load(std::memory_order_relaxed);
        if (cur < 0 || cur == s_logged) return;
        s_logged = cur;

        const int reason = cur >> 8;
        const int detail = cur & 0xFF;
        switch (reason) {
        case kShown:
            SKSE::log::info("Visibility: widget shown");
            break;
        case kManual:
            SKSE::log::info("Visibility: widget hidden - hide hotkey toggled");
            break;
        case kExternal:
            SKSE::log::info("Visibility: widget hidden - {} HideAll in force",
                            detail == static_cast<int>(iHUDBridge::Hider::kTFCam) ? "TFCam free camera"
                                                                                   : "iHUD Smart Hide");
            break;
        case kPaused:
            SKSE::log::info("Visibility: widget hidden - game paused (a pausing menu is open)");
            break;
        case kShowMenusOff:
            SKSE::log::info("Visibility: widget hidden - UI hidden (tm / Photo Mode Hide UI)");
            break;
        case kNo3D:
            SKSE::log::info("Visibility: widget hidden - player 3D not loaded (loading)");
            break;
        case kMenu:
            SKSE::log::info("Visibility: widget hidden - menu open: {}",
                            detail < static_cast<int>(std::size(kBlockMenus)) ? kBlockMenus[detail] : "?");
            break;
        case kAutoVanity:
            SKSE::log::info("Visibility: widget hidden - auto-vanity (idle) camera");
            break;
        case kCompass:
            SKSE::log::info("Visibility: widget hidden - following the compass hide (Visibility page option)");
            break;
        default:
            break;
        }
    }

    void QueueCompassPoll() {
        bool follow = true;
        { auto lk = Settings::Lock(); follow = Settings::Get().followCompassHide; }
        if (!follow) {
            g_compassResolved.store(false, std::memory_order_relaxed);
            g_compassHidden.store(false, std::memory_order_relaxed);
            return;
        }
        // One outstanding task at a time: during a load the main thread does not drain
        // SKSE tasks for seconds, and we must not pile up hundreds of polls behind it.
        if (g_pollQueued.exchange(true, std::memory_order_acq_rel)) return;
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) { g_pollQueued.store(false, std::memory_order_relaxed); return; }
        tasks->AddTask([]() {
            g_pollQueued.store(false, std::memory_order_relaxed);
            auto* ui = RE::UI::GetSingleton();
            auto* player = RE::PlayerCharacter::GetSingleton();
            // Nothing to read while the HUD movie may be mid-rebuild; keep the last state
            // but mark it unresolved so a stale "hidden" can't outlive a load.
            if (!ui || !player || !player->Is3DLoaded() ||
                ui->IsMenuOpen("Loading Menu") || ui->IsMenuOpen("Fader Menu") ||
                ui->IsMenuOpen("Main Menu") || !ui->IsMenuOpen("HUD Menu")) {
                g_compassResolved.store(false, std::memory_order_relaxed);
                return;
            }
            auto rd = ReadCompassAlpha();
            LogCompassChange(rd);
            g_compassHidden.store(rd.pathIdx >= 0 && rd.hidden, std::memory_order_relaxed);
            g_compassResolved.store(rd.pathIdx >= 0, std::memory_order_relaxed);
        });
    }
}
