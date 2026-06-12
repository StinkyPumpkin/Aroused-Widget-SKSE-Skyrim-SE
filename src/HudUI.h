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
}
