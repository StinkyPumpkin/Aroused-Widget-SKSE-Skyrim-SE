#include "WidgetController.h"

#include "ArousalReader.h"
#include "Settings.h"
#include "Visibility.h"

#include <atomic>
#include <chrono>
#include <thread>

// The controller now ONLY refreshes data sources on a timer. The HUD render
// functions (HudUI.cpp) read directly from cached state each frame; the
// controller does not push to any UI bridge.
//
// CrimeReader::GetPlayerBounty is fast enough to call from the render path
// directly, so we don't pre-cache it. ArousalReader requires async dispatch
// via DispatchStaticCall, so the controller fires Refresh() on a cadence.

namespace {
    std::atomic<bool> g_running{false};
    std::thread       g_thread;

    void Loop() {
        SKSE::log::info("WidgetController loop started");
        using namespace std::chrono;
        auto next_arousal = steady_clock::now();
        while (g_running.load(std::memory_order_relaxed)) {
            const auto now = steady_clock::now();
            int cadence;
            { auto lk = Settings::Lock(); cadence = Settings::Get().arousalCadenceSec; }
            if (cadence < 1) cadence = 1;
            if (now >= next_arousal) {
                ArousalReader::Refresh();
                next_arousal = now + seconds(cadence);
            }
            // v0.3.7: compass-follow state is read on the MAIN thread via an SKSE task
            // (Scaleform GetVariable from the render hook crashed - see Visibility.cpp).
            Visibility::QueueCompassPoll();
            std::this_thread::sleep_for(250ms);
        }
        SKSE::log::info("WidgetController loop stopped");
    }
}

namespace WidgetController {

    void Start() {
        if (g_running.exchange(true)) return;
        g_thread = std::thread(Loop);
    }

    void Stop() {
        g_running.store(false);
        if (g_thread.joinable()) g_thread.join();
    }
}
