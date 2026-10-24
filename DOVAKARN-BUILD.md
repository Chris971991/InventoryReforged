# Dovakarn's build of InventoryReforged.dll

Inventory Reforged for SkyUI 1.5.0 by LayMovi (Nexus 195826, file 824645). The first commit on this branch is the plugin
source exactly as the mod ships it in its archive's `source` folder (`plugin/`, `licences/`, `readme.txt`); everything
after it is Dovakarn's.

## What Dovakarn changed

- `plugin/equipment-panel/source/PlayerView.cpp` (`Hold`, `Restore`): the doll keeps the pose it was made in when it is
  copied again from the same body. The mod sets only the 26 bones of its standing pose on a copy; every other node
  (skirt and cloak bones, fingers, bones physics or idles move) is copied as the live body has it at that moment. For
  3 seconds after any change of gear the doll is copied every 300 ms (`Fresh`, so body mods that reshape a body a moment
  later are caught), so those parts jumped 2 to 3 times a second to wherever the live body had them. Under menus that
  keep the world running (Skyrim Souls RE) the doll looked like it updated at 2 FPS. Now each node named once keeps its
  turn and place from the copy before (a node a new piece brings is taken once), so a copy again from the same body
  looks the same unless the body itself changed shape. A new menu, or another body (a follower), starts from the body
  as it is.

## Build

- Visual Studio 2026 (MSVC, Ninja and CMake from Visual Studio), a vcpkg checkout at or after the preset's
  builtin-baseline `127402f1c75bb3d5ff6bce04b285faa4930a5aca` (built with `0c2a9ace19`, 23/10/2026).
- `plugin/equipment-panel/include/SKSEMenuFramework.h` is not in this repository: it is SKSE Menu Framework's header for
  mod authors, "SKSEMenuFramework Header" 3.15 (Nexus 120352, file 806270). Put it there before configuring.
- `build.cmd`: vcvars64, `VCPKG_ROOT`, then the mod's own preset: `cmake --preset skyrim-17` and `cmake --build build`
  in `plugin/equipment-panel`. Output: `plugin/equipment-panel/build/InventoryReforged.dll` (RelWithDebInfo,
  x64-windows-static-md: the same C runtime imports as the mod's own DLL).

The sha256 of the DLL Dovakarn serves is in Dovakarn's player file list and in the skyrp commit that adds it
(`server-admin/player-settings/SKSE/Plugins/InventoryReforged.dll`).
