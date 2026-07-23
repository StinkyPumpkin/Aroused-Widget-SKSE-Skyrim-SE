# Starting prompt for iHUD-Claude — paste into a new Claude Code session

Copy everything below the line and paste it as the first message of a fresh session.

---

Start the **iHUD-Claude** project — a pure-C++ SKSE replacement for Gopher's iHUD (Nexus mod 12440). Sibling to my **HUDWidgets-Claude** mod which shipped earlier today.

## Read these first (in order)

1. The wiki note with **locked scope, original feature list, my notes, and confirmed Scaleform paths**:
   `X:\MODDINGSSE\SkyrimObsidianVault\Skyrim\Skyrim\MODS\Mods\iHud Remake.md`

2. The detailed **planning doc** with phasing + file layout:
   `C:\dev\HUDWidgets-Claude\docs\iHUD-Claude-plan.md`

3. The **sibling project's wiki note** so you understand the architecture pattern, the `SKSE Menu Framework` header gotcha (lazy `GetModuleHandle`), the `"HUD Menu"` (with space) gotcha, the manual `SHGetKnownFolderPath` log-directory workaround, and the `JsonStore` persistence pattern:
   `X:\MODDINGSSE\SkyrimObsidianVault\Skyrim\Skyrim\MODS\Mods\HUD Widgets.md`

4. The **HUDWidgets-Claude codebase** is your reference architecture — mirror its layout:
   `C:\dev\HUDWidgets-Claude\` (CMakeLists.txt, CMakePresets.json, vcpkg.json, build.bat, src/plugin.cpp, src/Settings.{h,cpp}, src/JsonStore.{h,cpp}, src/HotkeyHandler.{h,cpp}, src/SettingsUI.{h,cpp}, include/SKSEMenuFramework.h)

## Project root + naming

Create `C:\dev\iHUDClaude\` mirroring HUDWidgets-Claude's layout. Mod folder: `X:\MODDINGSSE\modorganizer2\mods\iHUD--Claude\`. DLL name: `iHUDClaude.dll`. Plugin name: `iHUDClaude`.

## Phase 1 (this session)

Land a usable v0.1 plugin with **just two features**:

1. **Compass toggle hotkey** (iHUD original feature 1)
   - Default key: **X** (DX 45)
   - Optional modifier (Ctrl/Shift/Alt) — independent of the main key
   - **Bind UX: "Press a key to bind"** — MCM has a button that, when clicked, captures the next keypress and stores its DX code. NO scan-code memorization. (This is a hard requirement from my notes.)
   - Toggles `_alpha` on `_root.HUDMovieBaseInstance.CompassShoutMeterHolder` between 0 and stored "visible alpha" (start with 100)
   - Persist toggle state across saves? **No** — fresh session starts with compass visible
   - When compass hidden, ALSO hide `_root.HUDMovieBaseInstance.FloatingQuestMarker_mc` (this is feature 4 — cheap to bundle in Phase 1)

2. **Sneak meter remove option** (iHUD original feature 5)
   - MCM toggle: when on, hide `_root.HUDMovieBaseInstance.StealthMeterInstance` permanently (set `_visible=false` every frame in case the engine resets it)
   - Crosshair behavior NOT touched in Phase 1 — that's Phase 2

Do NOT do in Phase 1: conditional crosshair (#2), universal hide hotkey (#7), quest markers logic beyond the bundling above (#4).

## Architectural rules (from HUDWidgets-Claude lessons)

- **`RE::HUDMenu::MENU_NAME = "HUD Menu"` (with a SPACE).** `GetMovieView("HUDMenu")` returns null. Use the constant.
- **`SKSE::log::log_directory()` is broken on this rig** — returns `Skyrim.INI` instead of `Skyrim Special Edition`. Use the manual `SHGetKnownFolderPath(FOLDERID_Documents)` + literal `"Skyrim Special Edition"` pattern from `C:\dev\HUDWidgets-Claude\src\plugin.cpp`.
- **Patch the SKSE Menu Framework SDK header** — `static auto menuFramework = GetModuleHandle(...)` runs at DllMain. Our DLL loads alphabetically before SKSEMenuFramework, so it's null. Use the lazy-resolve pattern from `C:\dev\HUDWidgets-Claude\include\SKSEMenuFramework.h`.
- **Pure C++ — no Papyrus.** All Scaleform writes via `RE::UI::GetSingleton()->GetMovieView("HUD Menu")` then `view->SetVariable(path, GFxValue(value))`.
- **Settings via `JsonStore` pattern** — atomic write (.tmp + rename), .bak fallback, debounced 1s save thread.
- **Settings UI via SKSE Menu Framework** — `SetSection("iHUD")`, `AddSectionItem(...)` for each sub-page.
- **Hide iHUD pex / .esp** when this works — the two HUD-managers will fight over `_alpha` if both run.

## Verification before declaring v0.1 done

1. Press hotkey → compass + quest markers fade. Press again → return.
2. Sneak meter toggle on → never see sneak meter even when sneaking. Toggle off → reappears next sneak.
3. Settings persist across game restarts (config.json in `Data/SKSE/Plugins/iHUDClaude/`).
4. Log goes to `F:\Documents\My Games\Skyrim Special Edition\SKSE\iHUDClaude.log` (NOT `Skyrim.INI`).
5. MCP shows "iHUD" section with at least Layout / Hotkey sub-pages.

## What I'll send you for triage

After first launch I'll send `iHUDClaude.log` + the marker file at `X:\MODDINGSSE\modorganizer2\overwrite\SKSE\Plugins\iHUDClaude-startup.log` (use the same belt-and-suspenders marker pattern from HUDWidgets-Claude's plugin.cpp).

---

Start by reading the four reference files above, confirm you understand the lessons, then scaffold the project mirroring HUDWidgets-Claude. Don't go beyond Phase 1 in this session — we'll iterate Phases 2-4 separately.
