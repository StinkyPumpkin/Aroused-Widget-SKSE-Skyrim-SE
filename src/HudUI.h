#pragma once

// HUD render functions registered with SKSE Menu Framework's AddHudElement.
// Each render fn is called once per frame. They read from
// ArousalReader/CrimeReader cached state, plus Settings::Get() for
// position/scale/visibility, then issue ImGuiMCP draw calls.
namespace HudUI {
    // Loads textures + registers each widget's render function. Call from
    // kDataLoaded *after* Settings::Load() so the first frame has correct
    // positions.
    void Register();

    // 2026-09-12: one-line status of the auto-whoring source for the settings page
    // ("TDF Enhanced Prostitution.esp found" / "not installed - indicator inactive").
    const char* WhoringSourceStatus();

    // 0.4.0: true when at least one Advanced Nudity overlay (texture + AND faction) resolved.
    bool ANOverlaysAvailable();

    // 0.4.0: called by the Layout settings page every frame it is drawn; while fresh, the
    // separate AN widgets draw even when not flashed so they can be placed.
    void NoteLayoutPageOpen();
}
