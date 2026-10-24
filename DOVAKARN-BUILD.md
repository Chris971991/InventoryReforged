# Dovakarn's build of InventoryReforged.dll

Inventory Reforged for SkyUI 1.5.0 by LayMovi (Nexus 195826, file 824645). The first commit on this branch is the plugin
source exactly as the mod ships it in its archive's `source` folder (`plugin/`, `licences/`, `readme.txt`), and the commit
that adds `panel/` is the rest of that folder, also as shipped; everything else is Dovakarn's.

## What Dovakarn changed

All of it is built only with the CMake option `IR_DOVAKARN` (ON by default in this fork; OFF builds the mod as shipped,
which then needs `SKSEMenuFramework.h`, see Build), except the doll's pose, which is in every build.

- **The doll holds its pose** (`plugin/equipment-panel/source/PlayerView.cpp`, `Hold`, `Restore`, `WholeBodyMoves`): the
  doll keeps the pose it was made in when it is copied again from the same body. The mod sets only the 26 bones of its
  standing pose on a copy; every other node (skirt and cloak bones, fingers, bones physics or idles move) is copied as
  the live body has it at that moment. For 3 seconds after any change of gear the doll is copied every 300 ms (`Fresh`,
  so body mods that reshape a body a moment later are caught), so those parts jumped 2 to 3 times a second to wherever
  the live body had them. Under menus that keep the world running (Skyrim Souls RE) the doll looked like it updated at
  2 FPS. Now each node named once keeps its turn and place from the copy before (a node a new piece brings is taken
  once), so a copy again from the same body looks the same unless the body itself changed shape. The skeleton's `NPC`
  node and every node above it are always taken from the new copy: that is where body mods move the whole body (RaceMenu's
  high heels lift `NPC` by the heel height of the shoes worn), and a change of gear changes that for real. A new menu,
  or another body (a follower), starts from the body as it is.
- **No SKSE Menu Framework pages** (`ModMenu.cpp`, `Settings.cpp`): Dovakarn's online server has no mod menu. The build
  reads no `SKSE/Plugins/InventoryReforged/pages.json`, registers no section, page, key handler or test-stand window
  (`[Debug] bPagesWindow` is not read), and does not include `SKSEMenuFramework.h`. `ModMenu::Reload` (the settings read
  again when the journal closes) stays.
- **Hiding worn gear is refused** (`Hidden.cpp`, `SetOn`, `SetPieceOn`, `Read`, `ReadFollowers`): the panel's
  `SetHidden` and `SetHiddenPiece` calls can show a hidden slot or piece again but never hide one, whatever the panel
  movie or its settings ask, and no hidden slot or piece is read back from a save. Online, a hidden piece vanished in the
  player's own game only; every other player still drew it from what the server says they wear.
- **Skyrim 1.6.1170 only** (`main.cpp`, `SKSEPluginLoad`): on any other game version the plugin writes one line to
  `InventoryReforged.log` ("runs on Skyrim 1.6.1170 only, this game is ...") and does not load. It is built and checked
  for the one game version Dovakarn's server runs.

## Build

- Visual Studio 2022 or 2026 with the C++ tools (MSVC, CMake and Ninja come with it). Built with Visual Studio 2026.
- vcpkg at or after the preset's builtin-baseline `127402f1c75bb3d5ff6bce04b285faa4930a5aca`. `build.cmd` uses
  `VCPKG_ROOT` when it is set, otherwise it clones and bootstraps https://github.com/microsoft/vcpkg into `vcpkg\` next to
  itself on the first build (the vcpkg inside Visual Studio can be older than the baseline).
- `build.cmd`: finds Visual Studio with vswhere, runs vcvars64, then the mod's own preset in `plugin/equipment-panel`:
  `cmake --preset skyrim-17` and `cmake --build build`. Output: `plugin/equipment-panel/build/InventoryReforged.dll`
  (RelWithDebInfo, x64-windows-static-md: the same C runtime imports as the mod's own DLL).
- Only an `IR_DOVAKARN` OFF build (`cmake --preset skyrim-17 -DIR_DOVAKARN=OFF`) needs
  `plugin/equipment-panel/include/SKSEMenuFramework.h`, which is not in this repository: it is SKSE Menu Framework's
  header for mod authors, "SKSEMenuFramework Header" 3.15 (Nexus 120352, file 806270).

The sha256 of the DLL Dovakarn serves is in Dovakarn's player file list and in the commit of Dovakarn's server
repository that adds it (`server-admin/player-settings/SKSE/Plugins/InventoryReforged.dll`).
