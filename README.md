# Townfall Super Ultrawide (32:9) Mod

Removes the 21:9 gameplay restriction in *Silent Hill: Townfall* (Steam, UE 5.6.1)
so the full 32:9 viewport is rendered during gameplay, without stretching and
with the authored vertical FOV. Menus already supported 32:9; gameplay was
pillarboxed to 21:9.

Optionally, the in-game HUD can be constrained to a centered 16:9 (or 21:9)
safe area so all HUD elements are visible without turning your head. This
applies only during gameplay — main menu, inventory and pause screens always
use the full width.

## How it works

The game ships with full debug symbols (`Townfall-Win64-Shipping.pdb`), which
made it possible to pinpoint the exact engine functions responsible. The 21:9
clamp is enforced in **two coordinated places**, and the mod hooks both
(runtime pattern scanning only - the exe on disk is never modified, and if a
game update moves the code the hooks refuse to install):

1. **View-rect constraint** — `FViewport::CalculateViewExtents`
   (RVA `0x1798174`) shrinks the render rectangle (the black pillarbox bars)
   when the camera's aspect-ratio constraint binds. A hook hands the rect
   through unchanged for normal cameras (full 32:9); with
   `DisableInCutscenes=1` the original engine function runs for cutscene
   cameras, reproducing the vanilla pillarbox exactly.

2. **Projection matrix** — for cameras with `bConstrainAspectRatio`, UE builds
   the projection with the camera's *authored* aspect ratio
   (`UCameraComponent::AspectRatio`, e.g. 2.35) instead of the real view-rect
   aspect: `M00 = M11 / aspectProp`. With the bars gone that image would
   stretch across the whole 32:9 screen, so a hook on
   `FMinimalViewInfo::CalculateProjectionMatrixGivenView` clears
   `bConstrainAspectRatio` (bit 0 of the flag dword at FMinimalViewInfo+0x68)
   for normal cameras, making every camera project with
   `M00 = M11 / rectAspect`.

Cutscene cameras use constraint 1 (MaintainXFOV): the FOV property is the
horizontal FOV and the vertical FOV is derived from the real view rect, so
rendering them into the full 32:9 rect would crop the vertical FOV
(zoomed-in image). They are switched to constraint 0, which yields the
rect-independent authored vertical FOV `2*atan(tan(FOV/2)/aspectProp)` — the
picture only widens horizontally, identical to gameplay framing. With
`DisableInCutscenes=1` cutscene cameras are left 100% vanilla instead.

Combined result at 5120x1440: full-width rendering, horizontal FOV widens
exactly by the aspect ratio, and vertical FOV stays at the authored value —
identical vertical framing to the stock 21:9 mode, without bars or stretch.

**HUD constraint** The visible HUD is Slate/UMG parented under the
viewport overlay (`SOverlay`). A vtable hook on `SOverlay::OnArrangeChildren`
(slot 71) presents the overlay's children a modified `FGeometry` — a centered
box instead of the full 32:9 rect — so every HUD widget lays itself out
inside the box without touching a single draw call. The constraint is gated to
gameplay by watching `UTownfallGameInstance`'s game-state stack (captured via
a trampoline hook on `PopGameState`): only state 4 (gameplay) gets the box.

All hook sites are located at runtime by pattern scanning; the game exe on
disk is never modified. If a game update moves the code, the hooks refuse to
install and the game runs unmodified (enable `[Log] Enabled=1` and check
`TownfallUltraWide.log` next to the DLL).

## Install

1. Copy `dist\dxgi.dll` into
   `Townfall\Townfall\Binaries\Win64\`
2. Start the game normally (through Steam).

On first launch the mod creates `TownfallUltraWide.ini` with defaults next to
the DLL.

Uninstall: delete `dxgi.dll` (and optionally `TownfallUltraWide.log` /
`TownfallUltraWide.ini`). Steam file verification is not affected - the DLL is
not part of the game manifest.

The DLL is a proxy for the system `dxgi.dll` (so the game loads it
automatically); all DXGI calls are forwarded to the real system library,
loaded by full system path.

### Alternative proxy targets

`build.bat [target]` compiles the mod as a proxy for another system DLL and
generates the export stubs automatically from the real DLL's export table
(`tools\gen_proxy.py`). The proxy's export table is a full mirror of the
original (names, ordinals, ordinal-only exports), so the game cannot tell the
difference. Only use one proxy DLL at a time.

Tested targets (game exe `Townfall-Win64-Shipping.exe`, Win11 24H2) - the game
imports all of these, so each one is loaded from the game directory when used
as the proxy name:

| Target        | Loaded by game | Status                          |
|---------------|----------------|---------------------------------|
| `dxgi`        | static         | OK (default)                    |
| `winmm`       | static         | OK (180 named + 1 ordinal)      |
| `dsound`      | static         | OK                              |
| `dwmapi`      | static         | OK (44 named + 75 ordinal-only) |
| `bcrypt`      | static         | OK                              |
| `winhttp`     | static         | OK                              |
| `opengl32`    | static         | OK (368 exports)                |
| `d3d11`       | delay-load     | OK (hooks install on first use) |
| `d3d12`       | delay-load     | OK (hooks install on first use) |
| `xinput1_4`   | delay-load     | OK (8 named + 101 ordinal-only) |
| `mfreadwrite` | delay-load     | OK                              |

Not supported: `uxtheme`, `dbghelp`, `mf`, `shlwapi`, `crypt32`, `imm32`,
`gdi32`, `setupapi`, `shell32`, `ole32`, `oleaut32`, `ws2_32`, `advapi32`,
`version` - their exports are forwarded to other DLLs, which the generator
rejects because `GetProcAddress` is not guaranteed to resolve a forwarder.
`kernel32`/`user32`/`gdi32` additionally are KnownDLLs (Windows always loads
the system copy, an app-directory proxy is ignored).

A target only works if the game imports it (statically or delay-loaded) -
e.g. `dinput8` and `version` build fine but the game never loads them, so
they are useless as proxies. To check a new target: `build.bat <name>`; the
generator either emits stubs or explains why the DLL cannot be proxied.

## Config (optional)

`TownfallUltraWide.ini` next to the DLL (delete it to reset to defaults):

```ini
[Camera]
; 0 - off, 1 - on
; Unlocks super ultrawide (32:9) gameplay by removing the 21:9 aspect
; cap and pillarboxing. Vertical FOV stays as authored for 16:9, so
; the image is never stretched.
Enabled=1

; 0 - off, 1 - on
; Renders cutscenes like the unpatched game (pillarboxed to the
; camera's authored aspect) instead of full super ultrawide width.
DisableInCutscenes=0

[UI]
; Constrain: 0 - off, the HUD spans the full screen width
;            1 - constrain the in-game HUD to a centered 16:9 box
;            2 - constrain the in-game HUD to a centered 21:9 box
; Applies only during gameplay. Main menu, inventory and pause
; screens always use the full screen width.
Constrain=0

[Log]
; 0 - off, 1 - on
; Writes TownfallUltraWide.log next to the DLL. Only useful for
; troubleshooting; keep off during normal play.
Enabled=0
```

## Known notes

- Pre-rendered FMVs keep their baked-in bars regardless of the settings above.
- With `DisableInCutscenes=1` cutscenes are pixel-identical to the unpatched
  game (vanilla pillarbox); with `0` they render full 32:9 with the authored
  vertical FOV.

## Project layout

```
build.bat            - MSVC build script -> dist\dxgi.dll (build.bat <target>
                       builds a proxy for another system DLL, e.g. winmm)
build_all.bat        - builds all tested proxy targets into dist\
src/
  dllmain.cpp        - entry point, default-ini generation, config, init thread
  hooks.cpp          - camera hooks: conditional view-rect pass-through,
                       projection flag clear, cutscene FOV/vanilla handling
  uiconstraint.cpp   - HUD box: SOverlay arrange-hook + gameplay state gate
  detour.cpp/.h      - absolute-jump detour helper (r11-preserving trampolines)
  log.cpp/.h         - lock-free WriteFile logger (opt-in via [Log] Enabled)
  proxy.cpp/.h       - proxy core: real-DLL loader, resolve + trace helpers,
                       PROXY_STUB / PROXY_ORDINAL_STUB macros
tools/               - reversing + test toolchain
  gen_proxy.py       - generates the export stubs/.def for the proxy target
                       from the real system DLL's export table
  verify_exports.py  - compares a built proxy's export table with the real DLL
  resolve/dump_all/layout  - DbgHelp symbol tools (uses the shipped PDB)
  disasm/annotate/find_*   - capstone disassembly + xref scanners
  steam_boot_test.py       - Steam launch + window/brightness boot tester
```
