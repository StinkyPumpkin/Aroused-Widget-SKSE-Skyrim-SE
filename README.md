# OSL Aroused Widget

# Nexus Page - [OSL Aroused Widget](https://www.nexusmods.com/games/skyrimspecialedition/mods/185786)

A lightweight on-screen **arousal widget** for Skyrim SE/AE, made for **OSL Aroused**. It shows the player's current arousal as a scalable icon + numeric percentage, rendered as an ImGui overlay via **SKSE Menu Framework** — no Papyrus, no scripts, fully configurable in-game.

## Features

- On-screen arousal widget (icon + numeric %)
- Reads arousal from **OSL Aroused** (auto-detected; falls back to **SexLab Aroused / Redux**)
- Icon changes across arousal levels
- Two selectable image sets (aroused / exposure)
- Fully positionable and scalable in-game — no config-file editing
- Text scales with the widget
- Auto-hides with the compass (follows iHUD / Sandbox-When-Idle fades)
- iHUD‑Claude **Smart Hide** integration (hide, restore, or self-show above an arousal threshold) - ihudclaude not yet released
- Toggle hotkey
- Settings and position saved per profile (JSON)
- Pure SKSE C++ — no Papyrus, no scripts

## Requirements

- Skyrim Special Edition (SE or AE)
- [SKSE64](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- **SKSE Menu Framework** ([QTR-Modding](https://github.com/QTR-Modding/SKSE-Menu-Framework-3)) — provides the rendering + settings menu
- [OSL Aroused](https://www.nexusmods.com/skyrimspecialedition/mods/103621) (or SexLab Aroused / Redux) — the arousal source

## Installation

Grab the archive from [Releases](../../releases/latest) and install it with your mod manager (MO2 / Vortex). No configuration needed to start; the widget appears once an arousal framework is present.

## Configuration

Open the SKSE Menu Framework overlay → **Aroused Widget** section:

- Enable / disable the widget
- Position (X / Y) and scale / icon height
- Image set (aroused / exposure)
- Poll cadence
- "Follow compass hide" (auto-hide with the vanilla compass)

Settings persist to `Data/SKSE/Plugins/ArousedWidget/` per profile.

## Building (developers)

CMake + vcpkg + CommonLibSSE-NG, MSVC (VS BuildTools), Ninja.

```bat
:: from a VS Developer environment
cmake --preset release
cmake --build build/release
```

The `build.bat` helper configures the VS environment and auto-deploys the DLL.

## Credits

- **[QTR-Modding](https://github.com/QTR-Modding/SKSE-Menu-Framework-3)** — SKSE Menu Framework
- The **OSL Aroused** team — the arousal framework
- The **SKSE** team
