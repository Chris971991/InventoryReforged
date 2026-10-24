"""Build the equipment panel for SkyUI

    python -m interface.equipment.build
    python -m interface.equipment.build --debug

--debug builds the debug edition, which logs every step of the plugin and the panel: the panel takes the parts of
debug/ beside its own, the plugin comes from plugins/equipment-panel/build/debug (build.ps1 equipment-panel build/debug
IR_DEBUG_EDITION=ON) and the package goes to out/skyui-equipment/package-debug, so both editions sit side by side.
In the debug movie every function of the panel tells the plugin when it starts and ends, debug_calls.py puts the calls in.

The panel is a script movie of its own, Interface/InventoryReforged/panel.swf. The plugin loads it into the
inventory, container and barter menus of SkyUI as they open, so no file of SkyUI is shipped or replaced.
The parts in source run one after another in its first frame, 26-icons comes from interface.equipment.icons.
Beside it go the plugin from plugins/equipment-panel (built with plugins/build.ps1), the ini it ships with,
and the menu in SkyUI's own Mod Configuration: a script written from the pages in menu/config.json with the
starting values from the ini and the functions of menu/actions.psc, and a light plugin that holds its quest.
The same pages also make a menu for MCM Helper in out/skyui-equipment/menu-mcmhelper, the installer picks one,
and go into SKSE/Plugins/InventoryReforged/pages.json for the plugin to draw in SKSE Menu Framework.
Both menus get the pages with translation keys in place of the words, and Interface/Translations gets the
English of every word of the panel and the menus under each language of the game (interface.equipment.translations)
"""

import argparse
import configparser
import re
import json
import shutil
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

from common.paths import PROJECT
from interface import mcm, skyui_menu
from interface.equipment import debug_calls, icons, translations
from interface.swf import script_movie

SOURCE = Path(__file__).parent / "source"
MENU = Path(__file__).parent / "menu"
MENU_PAGES = MENU / "config.json"
MENU_ACTIONS = MENU / "actions.psc"
# the API for other mods, release.py puts it at the end of the archive's readme
API_TEXT = Path(__file__).parent / "api" / "InventoryReforged API.txt"
MENU_SCRIPT = "InventoryReforgedMenu"
MENU_PLUGIN = "InventoryReforged.esp"
NATIVE_SCRIPT = "InventoryReforged"
SETTINGS = "InventoryReforged.ini"
MENU_QUEST = 0x800
AUTHOR = "IWillCall"
WORK = PROJECT.parent / "out" / "skyui-equipment"
PACKAGE = WORK / "package" / "Inventory Reforged for SkyUI"
# the menu for a player with MCM Helper, the installer puts it in place of the one in the package
MCM_HELPER_MENU = WORK / "menu-mcmhelper"
# the pages the plugin draws in SKSE Menu Framework
FRAMEWORK_PAGES = Path("SKSE") / "Plugins" / "InventoryReforged" / "pages.json"
PLUGIN_FOLDER = PROJECT / "plugins" / "equipment-panel"
PLUGIN = PLUGIN_FOLDER / "build" / "InventoryReforged.dll"
SHIPPED = PLUGIN_FOLDER / "package"
MOVIE = Path("Interface") / "InventoryReforged" / "panel.swf"
# the eyes of the slots, a movie of their own the plugin draws over the 3D of the item menus
EYES_MOVIE = Path("Interface") / "InventoryReforged" / "eyes.swf"
EYES_PARTS = ["../overlay/eyes.as", "26-draw.as", icons]

PARTS = [
    "10-core.as",
    "12-compare.as",
    "20-rows.as",
    "21-marks.as",
    "22-sorting.as",
    "24-subcategories.as",
    "26-draw.as",
    icons,
    "27-queue.as",
    "28-junk.as",
    "28-junk-ask.as",
    "29-chip-menu.as",
    "30-panel.as",
    "31-strip.as",
    "32-popup.as",
    "33-context.as",
    "34-drag.as",
    "35-models.as",
    "36-window.as",
    "37-inspect.as",
    "38-stats.as",
    "38-hero.as",
    "38-look.as",
    "39-row-menu.as",
    "39-card-numbers.as",
    "39-gamepad.as",
    "39-browser-room.as",
    "39-companions.as",
    "39-magic.as",
    "40-start.as",
]

# the debug edition: its parts of the panel, the stamp they carry and where it's built
DEBUG = Path(__file__).parent / "debug"
# what the calls the debug build puts in every function go to, the movie's first part
CALLS = DEBUG / "09-calls.as"
VERSION_LINE = re.compile(r"project\(InventoryReforged VERSION ([\d.]+)")


@dataclass(frozen=True)
class Edition:
    plugin: Path
    package: Path
    # the menu for MCM Helper, beside the package
    helper_menu: Path
    # what the build makes on the way
    work: Path


RELEASE = Edition(PLUGIN, PACKAGE, MCM_HELPER_MENU, WORK / "build")
DEBUG_EDITION = Edition(PLUGIN_FOLDER / "build" / "debug" / PLUGIN.name, WORK / "package-debug" / PACKAGE.name, WORK / "menu-mcmhelper-debug",
                        WORK / "build-debug")


def debug_parts() -> list:
    """The parts with the debug ones: the log right after the core, the entries before the start, the menu search
    after it, the start defines what that one wraps"""
    parts = list(PARTS)
    parts.insert(parts.index("10-core.as") + 1, DEBUG / "11-log.as")
    parts.insert(parts.index("40-start.as"), DEBUG / "39-entries.as")
    parts.append(DEBUG / "41-finding.as")
    return parts


def title(part) -> str:
    """How the log names a part in the names of its functions"""
    return "26-icons.as" if part is icons else Path(part).name


def with_calls(named: list, parts: list[str]) -> list[str]:
    """The debug movie's parts: every function tells the plugin when it starts and ends, the calls' own part goes first"""
    instrumented, count = debug_calls.instrumented(parts, [title(part) for part in named])
    print(f"debug: {count} functions of the panel tell the plugin when they run")
    return [CALLS.read_text(encoding="utf-8")] + instrumented


def stamp() -> str:
    """What the debug panel tells the plugin it is, the plugin warns when its own version and edition differ"""
    version = VERSION_LINE.search((PLUGIN_FOLDER / "CMakeLists.txt").read_text(encoding="utf-8")).group(1)
    return f'Equipment.DEBUG_BUILD = "{version} debug {datetime.now():%Y-%m-%d %H:%M}";\n'


def placed(source: Path, target: Path) -> None:
    """A file of the package is hard linked into the stand, so it's replaced and never written through"""
    target.parent.mkdir(parents=True, exist_ok=True)
    target.unlink(missing_ok=True)
    shutil.copy2(source, target)


def scripts(parts: list = PARTS) -> list[str]:
    """A part is a name in source, a path of a debug part, or the icons made by icons.py"""
    return [part.script() if part is icons else (part if isinstance(part, Path) else SOURCE / part).read_text(encoding="utf-8") for part in parts]


def starts() -> dict[str, float]:
    """What every control of the pages starts from, as the shipped ini says"""
    told = configparser.ConfigParser(comment_prefixes=(";",))
    told.optionxform = str
    told.read(SHIPPED / "SKSE" / "Plugins" / SETTINGS, encoding="utf-8")
    found: dict[str, float] = {}
    for control in mcm.controls(MENU_PAGES):
        key, section = control.split(":")
        if not told.has_option(section, key):
            raise ValueError(f"the menu has {control}, the ini has no such setting")
        found[control] = float(told[section][key])
    return found


def written(source: Path, header: str) -> Path:
    """The menu script with its header and the functions the text controls call"""
    source.mkdir(parents=True, exist_ok=True)
    (source / f"{MENU_SCRIPT}.psc").write_text(header + "\n" + MENU_ACTIONS.read_text(encoding="utf-8"), encoding="utf-8")
    return source


def menu(target: Path, work: Path = RELEASE.work) -> list[str]:
    """The menu in SkyUI's own Mod Configuration: its script made from the pages, the plugin with its quest"""
    defaults = starts()
    shown = json.loads(MENU_PAGES.read_text(encoding="utf-8"))["displayName"]
    source = written(work / "menu", skyui_menu.script(keyed_pages(work), MENU_SCRIPT, shown, NATIVE_SCRIPT, defaults))
    mcm.compiled(MENU, NATIVE_SCRIPT, target)
    script = mcm.compiled(source, MENU_SCRIPT, target, also=(MENU,))
    esp = mcm.plugin(target / MENU_PLUGIN, MENU_PAGES, MENU_SCRIPT, MENU_QUEST, AUTHOR, shown)
    return [f"menu: {len(defaults)} settings in SkyUI's Mod Configuration, {script.name}, {esp.name}"]


def mcm_helper_menu(target: Path, work: Path = RELEASE.work) -> list[str]:
    """The same menu for a player with MCM Helper: the pages and their starting values for it to draw, an empty
    script on its base and the plugin with the quest under the mod's name, which MCM Helper finds its pages by.
    Both menus keep what is set in the same file, so changing from one to the other keeps the settings"""
    shutil.rmtree(target, ignore_errors=True)
    defaults = starts()
    pages = target / "MCM" / "Config" / mcm.mod_name(MENU_PAGES)
    placed(keyed_pages(work), pages / MENU_PAGES.name)
    sections: dict[str, list[str]] = {}
    for control, value in defaults.items():
        key, section = control.split(":")
        # whole numbers stay whole: a colour like 14069864 would come out as 1.40699e+07
        written_value = str(int(value)) if value == int(value) else f"{value:g}"
        sections.setdefault(section, []).append(f"{key} = {written_value}")
    (pages / "settings.ini").write_text("\n\n".join(f"[{section}]\n" + "\n".join(lines) for section, lines in sections.items()) + "\n", encoding="utf-8")
    header = (f"Scriptname {MENU_SCRIPT} extends MCM_ConfigBase\n"
              "{The menu of the mod for MCM Helper, it draws the pages of MCM/Config and keeps the values in MCM/Settings}\n")
    source = written(work / "menu-mcmhelper", header)
    mcm.compiled(MENU, NATIVE_SCRIPT, target)
    script = mcm.compiled(source, MENU_SCRIPT, target, also=(MENU,))
    esp = mcm.plugin(target / MENU_PLUGIN, MENU_PAGES, MENU_SCRIPT, MENU_QUEST, AUTHOR)
    return [f"menu for MCM Helper: {len(defaults)} settings, {script.name}, {esp.name} in {target}"]


def unknown(parts: list[str]) -> list[str]:
    """Names called or read as Equipment.name that no part ever sets: Scaleform calls a missing function silently"""
    text = "\n".join(parts)
    set_names = set(re.findall(r"Equipment\.(\w+)\s*=(?!=)", text))
    used = set(re.findall(r"Equipment\.(\w+)\s*\(", text))
    return sorted(used - set_names)


def keyed_pages(work: Path) -> Path:
    """The pages with translation keys, what both menus are made from"""
    return work / "pages" / "config.json"


def main(debug: bool = False) -> None:
    edition = DEBUG_EDITION if debug else RELEASE
    package = edition.package
    # the package starts empty, a file left from an older name would be installed beside the new one
    shutil.rmtree(package, ignore_errors=True)
    movie = package / MOVIE
    named = debug_parts() if debug else PARTS
    parts = scripts(named)
    if debug:
        at = named.index(DEBUG / "11-log.as")
        parts[at] = stamp() + parts[at]
    # the words for translation are read from the parts as written, the movie gets them with the calls in
    movie_parts = with_calls(named, parts) if debug else parts
    missing = unknown(movie_parts)
    if missing:
        raise ValueError(f"the scripts call functions nobody defines: {missing}")
    script_movie(movie, movie_parts, edition.work / "panel")
    print(f"movie: {movie} {movie.stat().st_size} bytes")
    eyes = package / EYES_MOVIE
    eye_parts = scripts(EYES_PARTS)
    missing = unknown(eye_parts)
    if missing:
        raise ValueError(f"the movie of the eyes calls functions nobody defines: {missing}")
    script_movie(eyes, eye_parts, edition.work / "eyes")
    print(f"movie of the eyes: {eyes} {eyes.stat().st_size} bytes")

    placed(edition.plugin, package / "SKSE" / "Plugins" / PLUGIN.name)
    placed(SHIPPED / "SKSE" / "Plugins" / SETTINGS, package / "SKSE" / "Plugins" / SETTINGS)
    print(f"plugin: {edition.plugin.stat().st_size} bytes")

    keyed, menu_words = translations.keyed_pages(json.loads(MENU_PAGES.read_text(encoding="utf-8")))
    pages = keyed_pages(edition.work)
    pages.parent.mkdir(parents=True, exist_ok=True)
    pages.write_text(json.dumps(keyed, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    # the plugin draws the same pages in SKSE Menu Framework when the game has it
    placed(pages, package / FRAMEWORK_PAGES)
    plugin_sources = [path.read_text(encoding="utf-8") for path in sorted((PLUGIN_FOLDER / "source").glob("*.cpp"))]
    entries = translations.lines(translations.panel_words(parts) + translations.plugin_words(plugin_sources), menu_words)
    files = translations.write(package, entries)
    print(f"translations: {len(entries)} lines in {len(files)} languages, {files[0].name}")
    for line in menu(package, edition.work) + mcm_helper_menu(edition.helper_menu, edition.work):
        print(line)
    # other mods compile against the calls of the native script, it goes where the Creation Kit of SE looks,
    # what the calls do and promise goes beside the mod's files
    placed(MENU / f"{NATIVE_SCRIPT}.psc", package / "Source" / "Scripts" / f"{NATIVE_SCRIPT}.psc")
    json.loads(MENU_PAGES.read_text(encoding="utf-8"))


if __name__ == "__main__":
    arguments = argparse.ArgumentParser(description="Builds the equipment panel for SkyUI")
    arguments.add_argument("--debug", action="store_true", help="the debug edition, its plugin and package in folders of their own")
    main(arguments.parse_args().debug)
