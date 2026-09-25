#include "iHUDBridge.h"

#include "ArousalReader.h"

#include <SKSE/SKSE.h>
#include <atomic>

namespace {
    // --Claude 2026-09-26 (0.4.0): ONE hide flag PER SENDER. Until 0.3.9 iHUDClaude and
    // TFCam shared a single g_pendingHide, and ANY RestoreAll also zeroed iHUD's arousal
    // threshold. Both senders broadcast the same message ids, so they cancelled each other:
    //   - iHUD's RestoreAll un-hid the widget in the middle of a TFCam free-camera session
    //     (ArousedWidget.log 2026-09-26: TFCam HideAll 07:51:28.885, iHUD RestoreAll
    //     07:53:00.176, TFCam's own RestoreAll only at 07:53:30.968).
    //   - TFCam's RestoreAll (every free-cam exit, incl. SexLab scene camera flips) wiped
    //     iHUD's RespectArousalThreshold while iHUD Smart Hide stayed armed. From then on
    //     iHUD's compass fade hid the widget even at max arousal, until the iHUD key was
    //     pressed off/on to resend the threshold - the "vanished at max arousal" report.
    std::atomic<bool>  g_ihudHide{false};
    std::atomic<bool>  g_tfcamHide{false};
    // iHUDClaude's RespectArousalThreshold. If > 0, a hide is only honoured while current
    // arousal is BELOW it (the widget stays up at high arousal). 0 = always honour the
    // hide. Cleared only by iHUDClaude's own RestoreAll.
    std::atomic<float> g_arousalThreshold{0.0f};

    enum class Sender { kIHUD, kTFCam };

    const char* SenderName(Sender a_sender) {
        return a_sender == Sender::kTFCam ? "TFCam" : "iHUDClaude";
    }

    void Handle(Sender a_sender, SKSE::MessagingInterface::Message* msg) {
        if (!msg) return;
        auto& hide = (a_sender == Sender::kTFCam) ? g_tfcamHide : g_ihudHide;
        switch (msg->type) {
        case iHUDBridge::kHideAll:
            hide.store(true, std::memory_order_relaxed);
            SKSE::log::info("iHUDBridge: HideAll from {}", SenderName(a_sender));
            break;

        case iHUDBridge::kRestoreAll:
            hide.store(false, std::memory_order_relaxed);
            if (a_sender == Sender::kIHUD) {
                g_arousalThreshold.store(0.0f, std::memory_order_relaxed);
            }
            SKSE::log::info("iHUDBridge: RestoreAll from {}", SenderName(a_sender));
            break;

        case iHUDBridge::kRespectArousalThreshold: {
            // iHUDClaude-only contract (TFCam's HUDHider sends HideAll/RestoreAll only).
            if (a_sender != Sender::kIHUD) {
                SKSE::log::warn("iHUDBridge: RespectArousalThreshold from {} ignored", SenderName(a_sender));
                break;
            }
            float threshold = 0.0f;
            if (msg->data && msg->dataLen >= sizeof(float)) {
                threshold = *static_cast<float*>(msg->data);
            }
            g_arousalThreshold.store(threshold, std::memory_order_relaxed);
            SKSE::log::info("iHUDBridge: RespectArousalThreshold {} from iHUDClaude", threshold);
            break;
        }

        default:
            SKSE::log::warn("iHUDBridge: unknown message type {} from {}", msg->type, SenderName(a_sender));
            break;
        }
    }

    void __cdecl CallbackIHUD(SKSE::MessagingInterface::Message* msg) { Handle(Sender::kIHUD, msg); }
    void __cdecl CallbackTFCam(SKSE::MessagingInterface::Message* msg) { Handle(Sender::kTFCam, msg); }
}

namespace iHUDBridge {

    void Register() {
        auto* mi = SKSE::GetMessagingInterface();
        if (!mi) {
            SKSE::log::error("iHUDBridge: no SKSE messaging interface");
            return;
        }
        if (!mi->RegisterListener("iHUDClaude", CallbackIHUD)) {
            SKSE::log::error("iHUDBridge: RegisterListener('iHUDClaude') failed");
            return;
        }
        // Also listen for TFCam (free camera HUD hide)
        mi->RegisterListener("TFCam", CallbackTFCam);
        SKSE::log::info("iHUDBridge: listener registered for senders 'iHUDClaude' + 'TFCam'");
    }

    void OnGameLoaded() {
        // A TFCam HideAll must not outlive its free-camera session. TFCam's ShowHUD returns
        // BEFORE it sends RestoreAll when the "HUD Menu" movie is missing (e.g. free cam
        // ending around a load), and since 0.4.0 an iHUD RestoreAll no longer clears TFCam's
        // flag, so a missed RestoreAll would hide the widget until the next free-cam cycle.
        // A loaded save / new game starts outside free camera, so drop TFCam's flag here.
        // iHUD's flag is deliberately kept: iHUD only re-sends HideAll on a Smart Hide
        // transition, so clearing it would show the widget while Smart Hide is still armed.
        if (g_tfcamHide.exchange(false, std::memory_order_relaxed)) {
            SKSE::log::info("iHUDBridge: cleared a TFCam HideAll left over from before the load");
        }
    }

    bool ArousalAboveRespectThreshold() {
        const float threshold = g_arousalThreshold.load(std::memory_order_relaxed);
        if (threshold <= 0.0f) return false;
        const auto arousal = ArousalReader::GetArousalCached();
        return arousal && static_cast<float>(*arousal) >= threshold;
    }

    Hider HiddenBy() {
        const bool ihud  = g_ihudHide.load(std::memory_order_relaxed);
        const bool tfcam = g_tfcamHide.load(std::memory_order_relaxed);
        if (!ihud && !tfcam) return Hider::kNone;
        // Threshold gating: if a threshold was set, only hide when current arousal is
        // below it. (Keep the widget visible at high arousal even when an external
        // hider says "hide all" - same rule as before, now per sender.)
        if (ArousalAboveRespectThreshold()) return Hider::kNone;
        return tfcam ? Hider::kTFCam : Hider::kIHUD;
    }

    bool IsHiddenByExternal() { return HiddenBy() != Hider::kNone; }
}
