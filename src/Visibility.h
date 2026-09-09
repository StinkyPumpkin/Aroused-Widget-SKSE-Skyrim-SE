#pragma once

namespace Visibility {
    // Should the HUD widgets paint this frame?
    // Hides during: blocking menus, paused state, auto-vanity camera, manual
    // hide toggle (hotkey).
    bool ShouldRender();

    // Queue a main-thread (SKSE task) read of the HUD compass alpha/visibility.
    // Called on a cadence by WidgetController; ShouldRender() only reads the cached
    // result. The Scaleform read must never run from the render callback (v0.3.7 fix).
    void QueueCompassPoll();

    // Toggle the manual-hide state (called by the hotkey handler).
    void ToggleManualHide();
    // Read current manual-hide state.
    bool IsManuallyHidden();
}
