# iHUD-Claude — Project Plan

**Goal:** Replace Gopher's iHUD (9 yrs old, Papyrus-only, doesn't know about modern HUD widgets) with a pure-C++ SKSE plugin that delivers iHUD's full feature set AND extends to hide third-party widgets (TrueHUD, Edge UI, STB Widgets, our HUDWidgets, etc.) via one configurable hotkey.

Sibling to HUDWidgets-Claude. Probably a separate DLL (different concern, different mod folder, different MCM section) but shares build infrastructure (vcpkg, CMake, SKSE Menu Framework).

---

## Architectural choices to lock before coding

| Question | Options | Recommendation |
|---|---|---|
| Same DLL or new plugin? | (a) Add to HUDWidgets-Claude (b) New `iHUD-Claude.dll` | **(b) New plugin.** Different concern, different mod folder, can be installed/uninstalled independently. |
| Replace iHUD entirely or coexist? | (a) Disable iHUD in MO2 (b) Run alongside | **(a) Replace.** Two HUD-managers fighting over `_alpha` is a recipe for flicker. Document that user disables iHUD when installing. |
| Settings UI | (a) SKSE Menu Framework (consistent with HUDWidgets) (b) MCM Helper (JSON) | **(a) SKSE Menu Framework** — matches HUDWidgets-Claude, no Papyrus, in-game live preview. |
| Persistence | `Data/SKSE/Plugins/iHUDClaude/config.json` | Same `JsonStore` pattern as HUDWidgets-Claude. |
| Hotkey input | `SKSEMenuFramework::AddInputEvent` | Same as HUDWidgets-Claude. |

---

## Feature breakdown — 6 + 1

### 1. Compass toggle hotkey (iHUD original feature 1)
- Hotkey, default `X` (DX 45) per iHUD's default
- Sets `_root.HUDMovieBaseInstance.CompassShoutMeterHolder._alpha` to 0/100
- MCM: hotkey rebind, default-on toggle (compass hidden at startup yes/no)

**Difficulty:** Easy. ~30 lines.

### 2. Conditional crosshair (iHUD original feature 2)
Crosshair visible only when:
- Player has weapon drawn AND it's ranged (bow/crossbow), OR
- Player has spell/staff equipped AND aiming, OR
- Player is hovering an activatable (door, item, NPC, etc.)

Hide otherwise (set `_root.HUDMovieBaseInstance.Crosshair._alpha = 0`).

**Signals:**
- Weapon out: `RE::PlayerCharacter::AsActorState()->IsWeaponDrawn()` + check equipped weapon's `WEAPON_TYPE`
- Spell aim: `RE::PlayerCharacter::GetMagicCaster(slot)->state` + `EquippedSpell`
- Activatable hover: `RE::PlayerCharacter::GetCrosshairRefHandle()` returns the hovered ref

**Difficulty:** Medium. State polling each frame; aggregates ~5 conditions into one bool.

### 3. Independent transparency for compass + crosshair (iHUD original feature 3)
- MCM sliders (0-100%) per element
- Multiplies the visible-state alpha (so "compass at 70%" means 70 when shown, 0 when hidden)

**Difficulty:** Easy. Just two floats.

### 4. Floating quest markers tied to compass (iHUD original feature 4)
When compass is hidden, also hide the floating world-space quest markers (the `>` arrows that float on objectives).

**Signals to investigate:**
- The marker layer in HUDMenu — research SkyUI source for path
- Possibly `_root.HUDMovieBaseInstance.QuestPointer` or similar

**Difficulty:** Medium. May need to grep SkyUI / HUDMenu sources to confirm path. May need to find the right mode (some marker layers are in WorldMap, not HUD).

### 5. Sneak meter remove option (iHUD original feature 5)
- Toggle: hide sneak meter entirely (`_alpha = 0`)
- Crosshair behaviour unchanged when bow/staff/spell aimed (i.e. crosshair logic from #2 still applies independently)

**Path:** `_root.HUDMovieBaseInstance.SneakMeter._alpha`

**Difficulty:** Easy.

### 6. Fast fade for stat bars at 100% (iHUD original feature 6)
Vanilla: when health/magicka/stamina returns to 100%, the bar fades over ~3 seconds. iHUD: shorten that to ~0.5s.

**Signals:**
- Hook stat-update: `RE::PlayerCharacter` actor-value listener? `BSAnimationGraphEvent`? Investigate.
- Or simpler: poll every frame, when stat == max AND was < max last frame, push fast fade.

Approach: drive the bar's existing tween via `Invoke` on `_root.HUDMovieBaseInstance.HealthMeter.fastFade()` if such a method exists in HudMenu's AS. Otherwise directly set _alpha with our own tween.

**Difficulty:** Medium. Stat hook needs research.

### 7 (NEW). Universal hide hotkey
Single hotkey hides EVERYTHING: vanilla HUD elements (compass, crosshair, sneak, stat bars), TrueHUD bars, Edge UI placeholders, STB widgets, HUDWidgets-Claude (our arousal widget), any future additions.

**Implementation:**
- Configurable list of (movieName, gfxPath) targets in JSON
- Default list ships with known paths for: TrueHUD, Edge UI, STB Widgets, HUDWidgets-Claude
- On hotkey: walk list, set each `_alpha = 0` (or call `setVisible(false)` if path supports it)
- Toggle restores

**For HUDWidgets-Claude integration:** since we already have a hide hotkey, expose it via SKSE messaging interface so iHUD-Claude can fire a "hide all" event and HUDWidgets-Claude listens.

**Difficulty:** Medium. The list management UI is the bulk of the work.

---

## Suggested phasing

**Phase 1: Skeleton + features 1, 5** (easy wins)
- Scaffold sibling project at `C:\dev\iHUDClaude\`
- Compass toggle hotkey (#1)
- Sneak meter remove (#5)
- Settings.cpp (config struct + JSON)
- Verify in-game

**Phase 2: Crosshair + transparency** (#2, #3)
- Conditional crosshair logic (weapon out + ranged + spell aim + activate hover)
- Transparency sliders for compass/crosshair

**Phase 3: Universal hide + HUDWidgets bridge** (#7)
- Configurable target list
- SKSE messaging integration with HUDWidgets-Claude

**Phase 4: Quest markers + fast fade** (#4, #6)
- Research signals/paths
- Implement

**Total estimate:** 4 sessions of ~2 hours each. Phase 1 standalone first, then iterate.

---

## Confirmed Scaleform paths (from iHUD pex inspection 2026-04-28)

All paths are children of `_root.HUDMovieBaseInstance` unless noted. Discovered via strings extraction from iHUD's `ihudcompassscript.pex`, `ihudcrosshairscript.pex`, `ihudhealthbarscript.pex`, `ihudwidgetscript.pex`, `ihudutilityscript.pex`.

| Element | GFx path | Notes |
|---|---|---|
| Compass | `CompassShoutMeterHolder` | Uses `_alpha` |
| Compass frame | `CompassShoutMeterHolder.Compass.CompassFrame` | Sub-element; transparency only |
| Compass direction rect | `CompassShoutMeterHolder.Compass.DirectionRect` | Sub-element; transparency only |
| Floating quest markers | `FloatingQuestMarker_mc` | Uses `_alpha`. Tied to compass visibility per iHUD |
| Crosshair main | `CrosshairInstance` | Uses both `_alpha` + `_visible` |
| Crosshair alert | `CrosshairAlert` | Sub overlay |
| Sneak meter | `StealthMeterInstance` | Uses `_visible` |
| Health bar | `Health` | Children: `Health.HealthMeter_mc`, `Health.HealthMeter_mc.HealthLeft` |
| Magicka bar | `Magicka` | Same shape as Health |
| Stamina bar | `Stamina` | Same shape as Health |
| Activate button | `ActivateButton_tf` | Activatable hover affordance |
| External widgets container | `_root.WidgetContainer` | NOT under HUDMovieBaseInstance — at root level |

**Pattern:** iHUD always sets `_alpha`. For hard on/off toggles it ALSO sets `_visible`. When rewriting we set both for safety on the toggled elements (compass, sneak meter, crosshair).

## Open research items

- [ ] Stat-bar fast-fade hook: actor-value event or poll?
- [ ] Crosshair-target reference: `GetCrosshairRefHandle()` confirmed in CommonLibSSE-NG?
- [ ] TrueHUD/Edge UI/STB Widget paths for the universal hide list (separate inspection — not iHUD)
- [ ] iHUD's actual default hotkey scan code (45 = X per its description, verify)
- [ ] How to verify path resolution at runtime for diagnostic UX (a "test hide" button in MCM that flashes the targeted clip?)

---

## File layout (tentative)

```
C:\dev\iHUDClaude\
├── CMakeLists.txt
├── CMakePresets.json
├── PCH.h
├── build.bat
├── vcpkg.json
├── vcpkg-configuration.json
├── include\
│   └── SKSEMenuFramework.h          # patched (lazy GetModuleHandle)
└── src\
    ├── plugin.cpp                    # entry, listener wiring
    ├── Settings.cpp/.h               # JSON config
    ├── JsonStore.cpp/.h              # ported from HUDWidgets-Claude
    ├── CompassToggle.cpp/.h          # feature #1
    ├── Crosshair.cpp/.h              # feature #2
    ├── SneakMeter.cpp/.h             # feature #5
    ├── StatBarFastFade.cpp/.h        # feature #6
    ├── QuestMarkers.cpp/.h           # feature #4
    ├── UniversalHide.cpp/.h          # feature #7
    ├── HotkeyHandler.cpp/.h          # all hotkey routing in one place
    └── SettingsUI.cpp/.h             # MCP UI
```

---

## Out of scope (for v1.0)

- Combat-only HUD show (auto-show stat bars when combat starts)
- Per-element fade speeds (configurable beyond on/off + transparency)
- Compass marker filtering (hide quest markers but keep location markers, etc.)
- Animated transitions (we just snap _alpha; no tween)

These are nice-to-haves for a hypothetical v1.1+.
