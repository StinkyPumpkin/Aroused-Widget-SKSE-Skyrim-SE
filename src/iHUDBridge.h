#pragma once

// Bridge for receiving messages from iHUDClaude.dll over SKSE messaging.
// iHUDClaude's "Universal Hide" feature dispatches to this listener so we
// hide our own widgets in coordination with the rest of the user's HUD.
namespace iHUDBridge {

    // Message types, must stay in sync with iHUDClaude's sender-side enum.
    enum Message : uint32_t {
        kHideAll                 = 1,  // hide all our widgets until RestoreAll
        kRestoreAll              = 2,  // restore visibility
        kRespectArousalThreshold = 3,  // payload: float; only hide if current arousal < threshold
    };

    // Register the SKSE messaging listener for sender "iHUDClaude".
    // Call from kPostLoad.
    void Register();

    // True iff iHUDClaude has sent kHideAll and not yet sent kRestoreAll
    // (modulated by kRespectArousalThreshold if applicable).
    bool IsHiddenByExternal();

    // --Claude 2026-09-15: true when a RespectArousalThreshold was sent AND current
    // arousal is at or above it, i.e. "keep the widget up regardless of who wants it
    // hidden". Exposed because the threshold has to apply to EVERY hide path, not
    // just iHUD's broadcast - see the compass-follow branch in Visibility.cpp.
    bool ArousalAboveRespectThreshold();
}
