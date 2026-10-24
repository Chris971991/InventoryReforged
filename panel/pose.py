"""The standing poses of the doll, from poses the plugin wrote down in the game

    python -m interface.equipment.pose [<folder with the pose files>]

With bDumpPose = 1 in the [Debug] section of its ini the plugin writes the bones of the live player into
SKSE/InventoryReforged-pose-male.txt or -female.txt each time the inventory opens: name|nine numbers of the
turn|three of the place. Taken while the player stands calm with nothing in the hands, they become
plugins/equipment-panel/source/StandingPose.h, the poses every doll is put in. The folder is the SKSE log folder
of the stand by default. A sex with no file keeps the pose the header has now. Only bones of the game's own
skeleton are kept, extra bones of a skeleton mod stay as they are on the doll
"""

import argparse
import math
import re
from pathlib import Path

from common.paths import PROJECT, STAND_INSTANCE

HEADER = PROJECT / "plugins" / "equipment-panel" / "source" / "StandingPose.h"
DUMPS = STAND_INSTANCE / "Documents" / "My Games" / "Skyrim Special Edition" / "SKSE"
OWN_PREFIX = "NPC "
SKIPPED = ("NPC L Pauldron", "NPC R Pauldron", "NPC L MagicNode", "NPC R MagicNode", "NPC Head MagicNode")
SEXES = {"male": "kMale", "female": "kFemale"}
# the idle turns the whole body a little around the upright axis, the doll faces the camera straight
FACING_BONE = "NPC COM [COM ]"


def straight(turn: list[float]) -> list[float]:
    """The turn with its part around the upright axis taken off"""
    rows = [turn[0:3], turn[3:6], turn[6:9]]
    yaw = math.atan2(rows[1][0], rows[0][0])
    cos, sin = math.cos(-yaw), math.sin(-yaw)
    undo = [[cos, -sin, 0.0], [sin, cos, 0.0], [0.0, 0.0, 1.0]]
    turned = [[sum(undo[r][k] * rows[k][c] for k in range(3)) for c in range(3)] for r in range(3)]
    return [value for row in turned for value in row]


def read(file: Path) -> list[str]:
    """Lines of the header for every bone of a pose file"""
    lines = []
    for line in file.read_text(encoding="utf-8").splitlines():
        name, turn, place = line.split("|")
        if not name.startswith(OWN_PREFIX) or name.startswith(SKIPPED):
            continue
        numbers = [float(value) for value in turn.split()]
        if name == FACING_BONE:
            numbers = straight(numbers)
        turn_values = ", ".join(f"{value:.6f}F" for value in numbers)
        place_values = ", ".join(f"{float(value):.4f}F" for value in place.split())
        lines.append(f"\t\t{{ \"{name}\"sv, {{ {turn_values} }}, {{ {place_values} }} }},")
    return lines


def kept() -> dict[str, list[str]]:
    """The bone lines the header holds now for each sex"""
    found = {name: [] for name in SEXES.values()}
    if not HEADER.exists():
        return found
    text = HEADER.read_text(encoding="utf-8")
    for name in SEXES.values():
        block = re.search(rf"{name}\{{ \{{\n(.*?)\n\t\}} \}};", text, re.DOTALL)
        if block:
            found[name] = block.group(1).splitlines()
    return found


def write(poses: dict[str, list[str]]) -> None:
    lines = [
        "#pragma once",
        "",
        "// made by python -m interface.equipment.pose from poses the plugin wrote in the game, don't edit by hand",
        "namespace StandingPose",
        "{",
        "\tstruct Bone",
        "\t{",
        "\t\tstd::string_view name;",
        "\t\tfloat turn[9];",
        "\t\tfloat place[3];",
        "\t};",
    ]
    for name, bones in poses.items():
        lines.append("")
        if bones:
            lines += [f"\tinline constexpr std::array<Bone, {len(bones)}> {name}{{ {{", *bones, "\t} };"]
        else:
            lines.append(f"\tinline constexpr std::array<Bone, 0> {name}{{}};")
    lines += ["}", ""]
    HEADER.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("folder", type=Path, nargs="?", default=DUMPS)
    arguments = parser.parse_args()
    poses = kept()
    for sex, name in SEXES.items():
        file = arguments.folder / f"InventoryReforged-pose-{sex}.txt"
        if file.exists():
            poses[name] = read(file)
            print(f"{sex}: {len(poses[name])} bones from {file}")
        else:
            print(f"{sex}: no file, {len(poses[name])} bones kept")
    write(poses)
    print(HEADER)


if __name__ == "__main__":
    main()
