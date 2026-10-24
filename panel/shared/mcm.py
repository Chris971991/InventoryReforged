"""What a menu drawn by MCM Helper needs beside its pages

MCM Helper draws the menu of a mod from MCM/Config/<mod>/config.json and keeps what the player sets in
MCM/Settings/<mod>.ini, a plugin of the mod reads that file. Beside the pages MCM Helper asks for
    a quest in a plugin that carries a script extending MCM_ConfigBase with the name of the mod,
      and a seat for the player with the load script of SkyUI                       quest()
    that script compiled, it is empty                                               compiled()
    the value every control starts from, MCM/Config/<mod>/settings.ini              the caller writes it
plugin() writes a light plugin that holds nothing but the quest, for a mod with no plugin of its own
The compiler knows MCM_ConfigBase from the stub in papyrus-headers, the game runs the script MCM Helper installs.
A menu in SkyUI's own Mod Configuration (interface.skyui_menu) uses the same quest and plugin with its own script
and the name the player sees
"""

import json
import struct
import subprocess
from pathlib import Path

from fixes.records_plugin import HEADER_FORM_VERSION, record
from mo2.outfit_patch import field
from npc_ube.plugin import LIGHT_PLUGIN, START_GAME_ENABLED_RUN_ONCE, group, text_field
from stand.uiprobe import CAPRICA, FLAGS, IMPORTS

HEADERS = Path(__file__).parent / "papyrus-headers"
QUEST = b"QUST"
MASTER = "Skyrim.esm"
PLAYER = 0x14
PLAYER_SEAT, PLAYER_SEAT_NAME = 0, "PlayerAlias"
# SkyUI tells a menu script through this one that a game was loaded
LOAD_SCRIPT = "SKI_PlayerLoadGameAlias"
# the script block of a record: its version, the way it writes objects, a text property that was set by hand
SCRIPT_BLOCK = struct.pack("<hh", 5, 2)
TEXT_PROPERTY, SET_BY_HAND = 2, 1
FRAGMENTS_VERSION = 2
OWN_FORMS = 1 << 24
HEADER_VERSION = 1.7


def worded(value: str) -> bytes:
    return struct.pack("<H", len(value)) + value.encode()


def mod_name(pages: Path) -> str:
    return json.loads(pages.read_text(encoding="utf-8"))["modName"]


def controls(pages: Path) -> list[str]:
    """The settings the pages show, each as "key:Section" """
    told = json.loads(pages.read_text(encoding="utf-8"))["pages"]
    return [control["id"] for page in told for control in page["content"] if "id" in control]


def quest(pages: Path, script: str, form: int, name: str | None = None) -> bytes:
    """The quest record of the menu, form is the number it gets in the plugin. SkyUI lists the menu under name"""
    name = name or mod_name(pages)
    one_script, one_property, no_properties, no_fragments, no_file_name, one_seat = 1, 1, 0, 0, 0, 1
    attached = (
        SCRIPT_BLOCK + struct.pack("<H", one_script)
        + worded(script) + struct.pack("<BH", 0, one_property)
        + worded("ModName") + bytes([TEXT_PROPERTY, SET_BY_HAND]) + worded(name)
        + struct.pack("<BHHH", FRAGMENTS_VERSION, no_fragments, no_file_name, one_seat)
        + struct.pack("<HhI", 0, PLAYER_SEAT, form)
        + SCRIPT_BLOCK + struct.pack("<H", one_script)
        + worded(LOAD_SCRIPT) + struct.pack("<BH", 0, no_properties)
    )
    body = (
        text_field(b"EDID", script)
        + field(b"VMAD", attached)
        + text_field(b"FULL", name)
        + field(b"DNAM", struct.pack("<HBBII", START_GAME_ENABLED_RUN_ONCE, 0, 0xFF, 0, 0))
        + field(b"NEXT", b"")
        + field(b"ANAM", struct.pack("<I", PLAYER_SEAT + 1))
        + field(b"ALST", struct.pack("<I", PLAYER_SEAT))
        + text_field(b"ALID", PLAYER_SEAT_NAME)
        + field(b"FNAM", struct.pack("<I", 0))
        + field(b"ALFR", struct.pack("<I", PLAYER))
        + field(b"VTCK", struct.pack("<I", 0))
        + field(b"ALED", b"")
    )
    return record(QUEST, form, body, 0, HEADER_FORM_VERSION)


def compiled(source: Path, script: str, folder: Path, also: tuple[Path, ...] = ()) -> Path:
    """Compiles source/<script>.psc into folder/Scripts, the scripts it calls are in source or in also"""
    scripts = folder / "Scripts"
    scripts.mkdir(parents=True, exist_ok=True)
    target = scripts / f"{script}.pex"
    # a file of a package is hard linked into the stand, it is replaced and never written through
    target.unlink(missing_ok=True)
    done = subprocess.run(
        [str(CAPRICA), "--game", "skyrim", "--import", ";".join(map(str, [*IMPORTS, HEADERS, *also, source])),
         "--flags", str(FLAGS), "--output", str(scripts), str(source / f"{script}.psc")],
        # the compiler names a script by its path from the folder it runs in, so it runs in the folder of the script
        capture_output=True, text=True, cwd=source,
    )
    if done.returncode or not target.is_file():
        raise ValueError("the menu script does not compile: " + (done.stderr + done.stdout).strip())
    return target


def plugin(target: Path, pages: Path, script: str, form: int, author: str, name: str | None = None) -> Path:
    """A light plugin with the quest of the menu and nothing else"""
    quests = [quest(pages, script, OWN_FORMS | form, name)]
    # the header counts records and groups alike, and names the first form still free
    header = field(b"HEDR", struct.pack("<fII", HEADER_VERSION, len(quests) + 1, form + 1))
    header += text_field(b"CNAM", author) + text_field(b"MAST", MASTER) + field(b"DATA", bytes(8))
    target.parent.mkdir(parents=True, exist_ok=True)
    target.unlink(missing_ok=True)
    target.write_bytes(record(b"TES4", 0, header, LIGHT_PLUGIN, HEADER_FORM_VERSION) + group(QUEST, quests))
    return target
