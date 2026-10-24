The source of Inventory Reforged for SkyUI

InventoryReforged.dll links CommonLibSSE-NG, which is published under the GNU General Public License 3 with a modding
exception and a linking exception. So the plugin is under the GPL 3 too and its source goes with every copy. The two
licence texts of CommonLibSSE-NG are in the folder "licences".

  plugin/equipment-panel     the SKSE plugin: CMakeLists.txt, CMakePresets.json, vcpkg.json and the folder source
  plugin/shared/source       C++ the plugin shares with Know your Skyrim (who holds what, people, places)
  plugin/cmake/ports         the vcpkg port of CommonLibSSE-NG 7.2.0 the plugin is built with
  panel/source               the panel itself, ActionScript 2 in parts. build.py joins them in the order of their
                             numbers into Interface/InventoryReforged/panel.swf
  panel/overlay              the movie of the eyes of the slots, eyes.swf, with the drawing parts of the panel
  panel/menu                 the pages of the menu for MCM Helper and SkyUI and their Papyrus scripts
  panel/tests                rules of the panel run outside the game: node panel/tests/logic.test.cjs
  panel/*.py, panel/shared   how the files are put together: icons.py draws the icons, pose.py writes the standing
                             pose, translations.py the files in Interface/Translations, build.py the package,
                             release.py this archive. They are parts of a larger set of scripts and kept for reading

To build the plugin: Visual Studio with the C++ tools, CMake, Ninja and the vcpkg that comes with Visual Studio
(VCPKG_ROOT pointing at it), then in plugin/equipment-panel
  cmake --preset skyrim-17
  cmake --build build
The movie is made with JPEXS Free Flash Decompiler: an empty movie gets the joined script as its first frame.
