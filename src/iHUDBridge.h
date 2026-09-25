#pragma once

// Bridge for receiving messages from iHUDClaude.dll (and TFCam) over SKSE messaging.
// iHUDClaude's "Universal Hide" feature dispatches to this listener so we
// hide our own widgets in coordination with the rest of the user's HUD.
namespace iHUDBridge {

    // Message types, must stay in sync with iHUDClaude's sender-side enum.
    enum Message : uint32_t {
        kHideAll                 = 1,  // hide all our widgets until RestoreAll
        kRestoreAll              = 2,  // restore visibility
        kRespectArousalThreshold = 3,  // payload: float; only hide if current arousal < threshold
    };

    // Who is currently hiding the widget (0.4.0: tracked per sender).
    enum class Hider : int {
        kNone  = 0,
        kIHUD  = 1,  // iHUDClaude Smart Hide
        kTFCam = 2,  // TFCam free camera HUD hide
    };

    // Register the SKSE messaging listeners for senders "iHUDClaude" and "TFCam".
    // Call from kPostLoad.
    void Register();

    // Which sender's HideAll is in force right now (each sender's HideAll is undone only
    // by that same sender's RestoreAll), modulated by kRespectArousalThreshold.
    Hider HiddenBy();

    // HiddenBy() != Hider::kNone.
    bool IsHiddenByExternal();

    // 0.4.0: kPostLoadGame / kNewGame - clear a TFCam HideAll whose RestoreAll never came
    // (free camera does not survive a load). iHUD's hide is left alone.
    void OnGameLoaded();

    // --Claude 2026-09-15: true when a RespectArousalThreshold was sent AND current
    // arousal is at or above it, i.e. "keep the widget up regardless of who wants it
    // hidden". Exposed because the threshold has to apply to EVERY hide path, not
    // just iHUD's broadcast - see the compass-follow branch in Visibility.cpp.
    bool ArousalAboveRespectThreshold();
}
