# Inventory Reforged for SkyUI: Dovakarn's build

This is the source of the `InventoryReforged.dll` that the Dovakarn Skyrim roleplay server gives its players. It is
Inventory Reforged for SkyUI 1.5.0 by LayMovi (https://www.nexusmods.com/skyrimspecialedition/mods/195826), from the
`source` folder of the mod's own archive, with Dovakarn's changes on top. The mod itself, its panel and its menus are
LayMovi's: get Inventory Reforged from its Nexus page.

The plugin links CommonLibSSE-NG, so it is licensed under the GNU General Public License version 3 (`LICENSE`), with
CommonLibSSE-NG's modding and linking exceptions (`licences/`).

## What Dovakarn changed

Branch `dovakarn/1.5.0`. The first commit and the commit adding `panel/` are the mod's source exactly as shipped; every
other commit is Dovakarn's. Details, with the files and functions: `DOVAKARN-BUILD.md`.

- The character in the inventory keeps its pose when it is copied again after a change of gear, so its skirts, cloaks
  and fingers no longer jump a few times a second. Body mods that move the whole body (RaceMenu's high heels) still
  follow new shoes.
- No settings pages in SKSE Menu Framework: the build reads no `pages.json` and does not need the framework's header.
- Hiding worn gear on the character is refused by the plugin itself, whatever the panel asks, and nothing hidden is
  read back from a save.
- The plugin loads on Skyrim 1.6.1170 only and logs why on any other version.

The changes other than the pose are behind the CMake option `IR_DOVAKARN` (ON here); `-DIR_DOVAKARN=OFF` builds the mod
as shipped. Dovakarn's changes to the panel movie (`panel.swf`: Hide taken out, Dovakarn's colours) are made on the
mod's built movie and are not part of this repository.

## Build

Visual Studio 2022 or 2026 with the C++ tools, git, then run `build.cmd`. It uses `VCPKG_ROOT` if set, otherwise clones
vcpkg next to itself on the first build. The DLL lands in `plugin/equipment-panel/build/InventoryReforged.dll`.

The mod's own notes on its source are in `readme.txt`.
