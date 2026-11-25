// OSL Aroused Widget - SKSE arousal widget for OSL Aroused
// Copyright (C) 2026 StinkyPumpkin
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
// even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program. If
// not, see <https://www.gnu.org/licenses/>.

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
