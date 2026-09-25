#pragma once

namespace Visibility {
    // Should the HUD widgets paint this frame?
    // Hides during: blocking menus, paused state, auto-vanity camera, manual
    // hide toggle (hotkey).
    //
    // a_menuOpen (0.4.0): SKSE Menu Framework's menu is open. The "soft" hides - iHUD /
    // TFCam HideAll, the auto-vanity camera, following the compass hide - are skipped
    // then, so the widget does not vanish while it is being positioned ("disappears in
    // the SKSE menu" report). The hard gates (hide hotkey, pause, tm / Photo Mode,
    // loading, blocking menus) still apply.
    bool ShouldRender(bool a_menuOpen = false);

    // Queue a main-thread (SKSE task) read of the HUD compass alpha/visibility.
    // Called on a cadence by WidgetController; ShouldRender() only reads the cached
    // result. The Scaleform read must never run from the render callback (v0.3.7 fix).
    void QueueCompassPoll();

    // 0.4.0: log which gate is hiding the widget, once per change. Called on
    // WidgetController's cadence (NOT the render thread); ShouldRender() only records.
    void LogHideReasonChange();

    // Toggle the manual-hide state (called by the hotkey handler).
    void ToggleManualHide();
    // Read current manual-hide state.
    bool IsManuallyHidden();
}
