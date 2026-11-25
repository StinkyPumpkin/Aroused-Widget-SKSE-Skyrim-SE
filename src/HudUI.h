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
