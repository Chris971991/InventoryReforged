"""Lay the equipment panel out the way a player gets it, as a folder with an installer for Mod Organizer 2 and
Vortex, and its archive

    python -m interface.equipment.release

Takes the package python -m interface.equipment.build writes and lays out two folders of out/skyui-equipment/release
  Inventory Reforged for SkyUI   the mod as one folder with the menu, as the installer gives it for its first answer
  installer         what goes into the archive
    core     the SKSE plugin, its ini and the panel script movie, installed always
    menu     the menu in the game drawn by SkyUI itself: the light plugin with its quest and the scripts
    menu-mcmhelper   the same menu drawn by MCM Helper, with its pages; the installer picks it when MCM Helper is there
    fomod    the installer and its picture
    source   what the plugin and the panel are built from. The plugin links CommonLibSSE-NG, which is under
             the GPL 3, so its source goes with every copy
    readme.txt
The archive goes beside the page texts as nexus/equipment-panel/InventoryReforged-<version>.zip
"""

import re
import shutil
from pathlib import Path

from PIL import Image

from common.paths import PROJECT
from interface.equipment.build import API_TEXT, MCM_HELPER_MENU, PACKAGE, WORK
from nexus import fomod

NAME = "Inventory Reforged for SkyUI"
FILES = "InventoryReforged"
AS_ONE_FOLDER = WORK / "release" / NAME
RELEASE = WORK / "release" / "installer"
NEXUS = PROJECT / "nexus" / "equipment-panel"
PICTURE = NEXUS / "screens" / "01-panel.jpg"
PICTURE_IN_INSTALLER = "fomod/main.jpg"
PICTURE_WIDTH = 960
VERSION_LINE = re.compile(rf"project\({FILES} VERSION ([\d.]+)")
SETUP_QUESTION = "Menu in the game"
MENU_FILES = (f"{FILES}.esp", "Scripts")
HELPER = "MCMHelper.esp"
HELPER_PLUGINS = ("SkyUI_SE.esp", HELPER)
MENU_BY_HELPER = "Menu in MCM Helper"
MENU_BY_SKYUI = "Menu in SkyUI"
NO_MENU = "Without the menu"
IN_GAME = ("setup", "menu")
BY_HELPER = ("setup", "mcmhelper")
BY_HAND = ("setup", "ini")

PLUGIN = PROJECT / "plugins" / "equipment-panel"
PANEL = PROJECT / "scripts" / "interface" / "equipment"
# folders of the archive's source and what goes into each, the plugin keeps its place beside the port it builds with
SOURCE = {
    "plugin/equipment-panel": [PLUGIN / name for name in ("CMakeLists.txt", "CMakePresets.json", "vcpkg.json", "source")],
    # the C++ the plugin shares with Know your Skyrim, CMakeLists.txt takes it from ../shared/source
    "plugin/shared": [PROJECT / "plugins" / "shared" / "source"],
    "plugin/cmake": [PROJECT / "plugins" / "cmake" / "ports"],
    "panel": [PANEL / name for name in ("source", "overlay", "menu", "tests", "build.py", "icons.py", "pose.py", "translations.py", "release.py")],
    "panel/shared": [PROJECT / "scripts" / "interface" / name for name in ("swf.py", "mcm.py", "skyui_menu.py")],
}
LICENCES = PLUGIN / "build" / "vcpkg_installed" / "x64-windows-static-md" / "share" / "commonlibsse-ng"


def version() -> str:
    return VERSION_LINE.search((PROJECT / "plugins" / "equipment-panel" / "CMakeLists.txt").read_text(encoding="utf-8")).group(1)


def installer() -> fomod.Installer:
    setup = fomod.Group(SETUP_QUESTION, (
        fomod.Option(
            MENU_BY_HELPER,
            "Installs the menu of the panel in System, Mod Configuration, drawn by MCM Helper. Picked when MCM Helper is "
            f"active. Adds the light plugin {FILES}.esp, it takes no place in your load order. A change in the menu holds "
            "the next time you open the inventory.",
            flag=BY_HELPER, picked_with=HELPER_PLUGINS),
        fomod.Option(
            MENU_BY_SKYUI,
            "Installs the menu of the panel in System, Mod Configuration, drawn by SkyUI itself, no MCM Helper needed. "
            f"Picked when MCM Helper isn't there. Adds the light plugin {FILES}.esp, it takes no place in your load order. "
            "Both menus keep what you set in the same file, changing from one to the other keeps it.",
            flag=IN_GAME, picked_without=(HELPER,)),
        fomod.Option(
            NO_MENU,
            f"Installs no plugin at all. The panel works the same, its switches live in Data/SKSE/Plugins/{FILES}.ini.",
            flag=BY_HAND),
    ))
    return fomod.Installer(
        name=NAME,
        author="IWillCall",
        version=version(),
        website="",
        description="Your character in 3D beside the SkyUI inventory, with a slot for everything you wear: click, drag, hide, compare and inspect.",
        folders=(fomod.Folder("core"), fomod.Folder("menu", installed_while=IN_GAME), fomod.Folder("menu-mcmhelper", installed_while=BY_HELPER)),
        steps=(fomod.Step("Setup", (setup,)),),
        image=PICTURE_IN_INSTALLER if PICTURE.exists() else None,
    )


def lay_out() -> None:
    shutil.rmtree(AS_ONE_FOLDER, ignore_errors=True)
    shutil.copytree(PACKAGE, AS_ONE_FOLDER)
    # one folder for a profile with MCM Helper, as the installer lays it out when MCM Helper is active
    shutil.copytree(MCM_HELPER_MENU, AS_ONE_FOLDER, dirs_exist_ok=True)
    shutil.rmtree(RELEASE, ignore_errors=True)
    for file in (path for path in PACKAGE.rglob("*") if path.is_file()):
        inside = file.relative_to(PACKAGE)
        # the script of the plugin's calls is the API other mods call, it goes with every way of installing
        menu = inside.parts[0] in MENU_FILES and inside.name != f"{FILES}.pex"
        target = RELEASE / ("menu" if menu else "core") / inside
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(file, target)
    shutil.copytree(MCM_HELPER_MENU, RELEASE / "menu-mcmhelper")
    # the API for other mods is the last part of the readme, written once in api/
    readme = (NEXUS / "readme.txt").read_text(encoding="utf-8").rstrip("\n")
    api = API_TEXT.read_text(encoding="utf-8")
    (RELEASE / "readme.txt").write_text(f"{readme}\n\n{api}", encoding="utf-8")
    lay_source()
    if PICTURE.exists():
        shown = RELEASE / PICTURE_IN_INSTALLER
        shown.parent.mkdir(parents=True)
        with Image.open(PICTURE) as picture:
            height = round(picture.height * PICTURE_WIDTH / picture.width)
            picture.convert("RGB").resize((PICTURE_WIDTH, height), Image.LANCZOS).save(shown, quality=88)


def lay_source() -> None:
    here = RELEASE / "source"
    for folder, parts in SOURCE.items():
        for part in parts:
            target = here / folder / part.name
            if part.is_dir():
                shutil.copytree(part, target, ignore=shutil.ignore_patterns("__pycache__"))
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(part, target)
    (here / "licences").mkdir()
    shutil.copyfile(LICENCES / "copyright", here / "licences" / "CommonLibSSE-NG-LICENSE.txt")
    shutil.copyfile(LICENCES / "EXCEPTIONS.md", here / "licences" / "CommonLibSSE-NG-EXCEPTIONS.md")
    shutil.copyfile(NEXUS / "source-readme.txt", here / "readme.txt")


def main() -> None:
    lay_out()
    told = installer()
    fomod.write(RELEASE, told)
    missing = fomod.check(RELEASE)
    for way in (MENU_BY_HELPER, MENU_BY_SKYUI, NO_MENU):
        answers = {SETUP_QUESTION: [way]}
        folders = fomod.picked(told, answers)
        missing += [folder for folder in folders if not (RELEASE / folder).is_dir()]
        print(f"{way}: {', '.join(folders)}")
    if missing:
        raise SystemExit(f"the installer names what the release does not have: {missing}")
    archive = shutil.make_archive(str(NEXUS / f"{FILES}-{told.version}"), "zip", RELEASE)
    files = sum(1 for file in RELEASE.rglob("*") if file.is_file())
    print(f"{files} files in {RELEASE}")
    print(f"{archive}: {Path(archive).stat().st_size} bytes")


if __name__ == "__main__":
    main()
