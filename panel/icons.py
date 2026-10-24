"""Vector icons of the equipment panel: slot silhouettes and the small signs of buttons

    python -m interface.equipment.icons            a sheet of all icons in out/skyui-equipment/icons.png
    python -m interface.equipment.icons --choose a b c
                                                   candidates as the bar button draws them, out/skyui-equipment/icon-choice.png

Every icon is drawn in a box of 24 by 24 from SVG paths with absolute M L H V Q C Z commands. A fill
path follows the even-odd rule the Flash drawing API uses, so a contour inside another one cuts a hole.
A stroke is a line of a given width. script() writes the icons as numbers for 26-icons.as, the menu
draws them with its drawing API at any size and colour
"""

import math
import re
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

from common.paths import PROJECT

SHEET = PROJECT.parent / "out" / "skyui-equipment" / "icons.png"
CHOICE = PROJECT.parent / "out" / "skyui-equipment" / "icon-choice.png"
BOX = 24.0
GOLD, PARCHMENT = (214, 176, 104), (230, 224, 200)
# wide, high, sign x, sign y, sign size of the browser's bar button, as 25-button.as of Know your Skyrim draws it
BAR_BUTTON = (32, 22, 5, 0, 22)
MOVE, LINE, CURVE = 0, 1, 2


def circle(x: float, y: float, r: float) -> str:
    """A closed circle out of four cubic quarters"""
    k = 0.5523 * r
    return (f"M {x + r} {y} C {x + r} {y + k} {x + k} {y + r} {x} {y + r} C {x - k} {y + r} {x - r} {y + k} {x - r} {y} "
            f"C {x - r} {y - k} {x - k} {y - r} {x} {y - r} C {x + k} {y - r} {x + r} {y - k} {x + r} {y} Z")


# A vertical sword, turned on its side by the icon's turn
SWORD = [
    ("fill", "M 12 0.8 L 13.7 4 L 13.7 15.2 L 10.3 15.2 L 10.3 4 Z "
             "M 11.65 4.6 L 12.35 4.6 L 12.35 14.4 L 11.65 14.4 Z"),
    ("fill", "M 6.6 15.4 C 8 15 16 15 17.4 15.4 L 17.4 17 C 16 16.6 8 16.6 6.6 17 Z"),
    ("fill", "M 11 17.2 L 13 17.2 L 13 21.2 L 11 21.2 Z"),
    ("fill", circle(12, 22.3, 1.5)),
]

SHIELD = [
    ("fill", "M 12 1.8 C 15 3.1 18 3.5 20.6 3.3 L 20.6 10 C 20.6 15.6 17 19.6 12 22.2 C 7 19.6 3.4 15.6 3.4 10 L 3.4 3.3 C 6 3.5 9 3.1 12 1.8 Z "
             "M 12 4 C 14.6 5 17 5.4 18.9 5.3 L 18.9 10 C 18.9 14.6 16.1 17.9 12 20.2 C 7.9 17.9 5.1 14.6 5.1 10 L 5.1 5.3 C 7 5.4 9.4 5 12 4 Z "
             "M 11.2 5 L 12.8 5 L 12.8 9.2 L 18.6 9.2 L 18.6 10.8 L 12.8 10.8 L 12.8 19.4 L 11.2 19.4 L 11.2 10.8 L 5.4 10.8 L 5.4 9.2 L 11.2 9.2 Z"),
]

# Two arrows that cross, each one a shaft, a head and a fletching
ARROW = [
    ("stroke", 1.3, "M 12 6 L 12 21"),
    ("fill", "M 12 1 L 14.6 7.2 L 12 6.2 L 9.4 7.2 Z"),
    ("fill", "M 12 17.5 L 14.4 15.6 L 14.4 19 L 12 21.4 Z M 12 17.5 L 9.6 15.6 L 9.6 19 L 12 21.4 Z"),
]

AMULET = [
    ("stroke", 1.1, "M 4.5 1.5 C 5 7 9 10 12 10.5 C 15 10 19 7 19.5 1.5"),
    ("fill", circle(12, 11.2, 1.4) + " " + circle(12, 11.2, 0.6)),
    ("fill", "M 12 12.6 C 14.6 13.6 16.4 15.4 16.4 17.4 C 16.4 20.2 14.2 22.4 12 22.6 C 9.8 22.4 7.6 20.2 7.6 17.4 C 7.6 15.4 9.4 13.6 12 12.6 Z "
             "M 12 15 L 14.2 17.6 L 12 20.6 L 9.8 17.6 Z"),
]

RING = [
    ("fill", circle(12, 14.6, 7.4) + " " + circle(12, 14.6, 5.4)),
    ("fill", "M 12 1.4 L 15.4 4.6 L 12 7.4 L 8.6 4.6 Z M 12 2.9 L 13.8 4.6 L 12 6 L 10.2 4.6 Z"),
]

HELMET = [
    ("fill", "M 3.8 13.4 C 3.8 7.2 7.4 2.8 12 2.8 C 16.6 2.8 20.2 7.2 20.2 13.4 L 20.2 19.4 L 16.6 21.6 L 15.6 15.4 L 13.1 15.4 L 13.1 21.4 L 10.9 21.4 L 10.9 15.4 L 8.4 15.4 L 7.4 21.6 L 3.8 19.4 Z "
             "M 4.6 12.2 L 19.4 12.2 L 19.4 13 L 4.6 13 Z "
             "M 11.6 3.6 L 12.4 3.6 L 12.4 11.6 L 11.6 11.6 Z"),
]

CUIRASS = [
    ("fill", "M 6.4 2.6 L 9.6 4.2 C 10.6 5.8 13.4 5.8 14.4 4.2 L 17.6 2.6 L 21.6 5.6 L 20.2 10.6 L 18 10 L 18 18.4 C 16 21 8 21 6 18.4 L 6 10 L 3.8 10.6 L 2.4 5.6 Z "
             "M 6.7 14.8 L 17.3 14.8 L 17.3 15.7 L 6.7 15.7 Z "
             "M 11.6 6.8 L 12.4 6.8 L 12.4 14.2 L 11.6 14.2 Z"),
]

GAUNTLET = [
    ("fill", "M 6.6 22.4 L 17.2 22.4 L 17.6 17.6 L 18.2 12.4 L 18.2 6.6 C 18.2 5.4 16.6 5.4 16.6 6.6 L 16.6 10.6 L 16 10.6 L 16 4.6 C 16 3.4 14.4 3.4 14.4 4.6 L 14.4 10.4 L 13.8 10.4 L 13.8 3.8 C 13.8 2.6 12.2 2.6 12.2 3.8 L 12.2 10.4 L 11.6 10.4 L 11.6 5 C 11.6 3.8 10 3.8 10 5 L 10 12.6 L 9.2 12.9 L 7.4 10 C 6.8 9 5.2 9.6 5.8 10.8 L 8 15.6 L 6.2 17.6 Z "
             "M 7.2 18.8 L 16.8 18.8 L 16.8 19.6 L 7.2 19.6 Z"),
]

BOOT = [
    ("fill", "M 5.6 1.8 L 13.4 1.8 L 13.4 12.6 C 13.4 13.8 14.4 14.8 15.8 15.1 L 19.8 16 C 21.4 16.4 22 17.8 22 19 L 22 21.6 L 5.6 21.6 Z "
             "M 6.4 19.2 L 21.2 19.2 L 21.2 19.9 L 6.4 19.9 Z "
             "M 6.4 4.4 L 12.6 4.4 L 12.6 5.1 L 6.4 5.1 Z"),
]

CLOAK = [
    ("fill", "M 8.8 2.4 L 15.2 2.4 C 15.8 4 17.2 5 18.8 5.8 C 20.2 10 21.2 16 22.2 22.2 L 1.8 22.2 C 2.8 16 3.8 10 5.2 5.8 C 6.8 5 8.2 4 8.8 2.4 Z "
             + circle(12, 6.4, 1.7) + " " + circle(12, 6.4, 0.8) + " "
             "M 8.6 10 L 9.4 10 L 7.6 21.4 L 6.8 21.4 Z M 15.4 10 L 14.6 10 L 16.4 21.4 L 17.2 21.4 Z "
             "M 11.6 9.6 L 12.4 9.6 L 12.4 21.4 L 11.6 21.4 Z"),
]

# Signs of the subcategories that no slot has: a battleaxe for two handed, a bow, a staff, light armor as a
# laced jerkin, heavy armor as a plate with pauldrons, clothing as a robe
BATTLEAXE = [
    ("fill", "M 11.2 1.8 L 12.8 1.8 L 12.8 23 L 11.2 23 Z"),
    ("fill", "M 11 4.2 C 8.2 3.6 5 3.6 2.8 2.4 C 1.8 6.2 1.8 10.4 2.8 14.2 C 5 13 8.2 12.8 11 12.2 Z"),
    ("fill", "M 13 4.2 C 15.8 3.6 19 3.6 21.2 2.4 C 22.2 6.2 22.2 10.4 21.2 14.2 C 19 13 15.8 12.8 13 12.2 Z"),
]

BOW = [
    ("stroke", 2.0, "M 15.6 1.4 C 5.4 5 5.4 19 15.6 22.6"),
    ("stroke", 0.8, "M 15.6 1.4 L 15.6 22.6"),
    ("stroke", 1.1, "M 4.6 12 L 19.4 12"),
    ("fill", "M 1.6 12 L 5.6 10 L 5.6 14 Z"),
    ("fill", "M 17.6 12 L 20.6 10 L 21.6 10 L 19.6 12 L 21.6 14 L 20.6 14 Z"),
]

STAFF = [
    ("fill", "M 11.2 8.6 L 12.8 8.6 L 12.5 23 L 11.5 23 Z"),
    ("stroke", 1.2, "M 9.2 9.6 C 7.6 7 8.2 3.4 10.6 1.8"),
    ("stroke", 1.2, "M 14.8 9.6 C 16.4 7 15.8 3.4 13.4 1.8"),
    ("fill", circle(12, 5.4, 2.6)),
]

JERKIN = [
    ("fill", "M 7 2.6 L 10.2 3.6 L 12 7.6 L 13.8 3.6 L 17 2.6 L 19.2 6.4 L 17.8 9.8 L 17.8 20 C 15 21.6 9 21.6 6.2 20 L 6.2 9.8 L 4.8 6.4 Z "
             "M 12 9.4 L 13.1 10.6 L 12 11.8 L 10.9 10.6 Z M 12 13 L 13.1 14.2 L 12 15.4 L 10.9 14.2 Z "
             "M 12 16.6 L 13.1 17.8 L 12 19 L 10.9 17.8 Z"),
]

PLATE = [
    ("fill", "M 7.4 4.8 C 9.6 6.2 14.4 6.2 16.6 4.8 L 17.6 8.8 L 17.6 18.2 C 15.6 21.4 8.4 21.4 6.4 18.2 L 6.4 8.8 Z "
             "M 6.9 12.4 L 17.1 12.4 L 17.1 13.3 L 6.9 13.3 Z M 7.1 15.6 L 16.9 15.6 L 16.9 16.5 L 7.1 16.5 Z"),
    ("fill", "M 6.8 4 C 4 3.2 1.8 5.2 1.6 9.6 L 5.6 10.4 L 6.4 6.6 Z"),
    ("fill", "M 17.2 4 C 20 3.2 22.2 5.2 22.4 9.6 L 18.4 10.4 L 17.6 6.6 Z"),
]

ROBE = [
    ("fill", "M 9 2.4 L 15 2.4 L 16.2 4.2 L 21.8 9.6 L 19.6 12 L 16.6 9.6 L 18.2 22 L 5.8 22 L 7.4 9.6 L 4.4 12 L 2.2 9.6 L 7.8 4.2 Z "
             "M 10.6 3.2 L 13.4 3.2 L 12 6.8 Z M 7.2 12.6 L 16.8 12.6 L 16.9 13.6 L 7.1 13.6 Z"),
]

# Signs of the lines of the item menu a right click opens on a row
STAR = [
    ("fill", "M 12 2 L 14.53 9.02 L 21.99 9.26 L 16.09 13.83 L 18.17 20.99 L 12 16.8 L 5.83 20.99 L 7.91 13.83 "
             "L 2.01 9.26 L 9.47 9.02 Z"),
]

DROP = [
    ("fill", "M 10.8 2.5 L 13.2 2.5 L 13.2 11.2 L 10.8 11.2 Z"),
    ("fill", "M 6.4 11 L 17.6 11 L 12 17.6 Z"),
    ("fill", "M 3 19.8 L 21 19.8 L 21 22 L 3 22 Z"),
]

TRANSFER = [
    ("fill", "M 3 6.9 L 15 6.9 L 15 9.1 L 3 9.1 Z"),
    ("fill", "M 14.4 3.8 L 21 8 L 14.4 12.2 Z"),
    ("fill", "M 9 14.9 L 21 14.9 L 21 17.1 L 9 17.1 Z"),
    ("fill", "M 9.6 11.8 L 3 16 L 9.6 20.2 Z"),
]

COIN = [
    ("fill", circle(12, 12, 9.8) + " " + circle(12, 12, 8.2)),
    ("fill", circle(12, 12, 6.4) + " M 11 8.2 L 13 8.2 L 13 15.8 L 11 15.8 Z"),
]

POTION = [
    ("fill", "M 10 2.4 L 14 2.4 L 14 4 L 13.4 4 L 13.4 8.2 C 17.2 9.3 19.5 12.3 19.5 15.6 C 19.5 19.8 16.2 22.2 12 22.2 "
             "C 7.8 22.2 4.5 19.8 4.5 15.6 C 4.5 12.3 6.8 9.3 10.6 8.2 L 10.6 4 L 10 4 Z "
             "M 6.2 13.8 L 17.8 13.8 L 17.9 14.9 L 6.1 14.9 Z"),
]

BOOK = [
    ("fill", "M 2 5 C 5.5 4 9 4.5 11.3 6.4 L 11.3 20.4 C 9 18.6 5.5 18.2 2 19.2 Z"),
    ("fill", "M 22 5 C 18.5 4 15 4.5 12.7 6.4 L 12.7 20.4 C 15 18.6 18.5 18.2 22 19.2 Z"),
]

GEM = [
    ("fill", "M 8 3 L 16 3 L 21 9 L 12 21.6 L 3 9 Z M 4.2 8.3 L 19.8 8.3 L 19.8 9.3 L 4.2 9.3 Z "
             "M 9 9.3 L 10.1 9.3 L 12 18 L 11.4 18.6 Z"),
]

AMOUNT = [
    ("fill", "M 2.5 10.9 L 21.5 10.9 L 21.5 13.1 L 2.5 13.1 Z"),
    ("fill", circle(9, 12, 4.4) + " " + circle(9, 12, 2.2)),
    ("fill", "M 2.5 17.5 L 3.7 17.5 L 3.7 20 L 2.5 20 Z M 11.4 17.5 L 12.6 17.5 L 12.6 20 L 11.4 20 Z "
             "M 20.3 17.5 L 21.5 17.5 L 21.5 20 L 20.3 20 Z"),
]

# Signs of the subcategories of potions, food, books and miscellaneous things
POISON = [
    ("fill", "M 9.6 2.4 L 14.4 2.4 L 14.4 4 L 13.6 4 L 13.6 7.4 L 17.6 14.6 C 18.8 16.8 17.4 21.6 12 21.6 C 6.6 21.6 5.2 16.8 6.4 14.6 "
             "L 10.4 7.4 L 10.4 4 L 9.6 4 Z " + circle(10.2, 15.6, 1.3) + " " + circle(13.8, 15.6, 1.3) + " "
             "M 11.2 18.2 L 12.8 18.2 L 12.8 19.4 L 11.2 19.4 Z"),
]

MUG = [
    ("fill", "M 4 5 L 15.6 5 L 15.6 20.6 C 15.6 21.4 15 22 14.2 22 L 5.4 22 C 4.6 22 4 21.4 4 20.6 Z "
             "M 6.4 8.4 L 7.6 8.4 L 7.6 19 L 6.4 19 Z M 9.2 8.4 L 10.4 8.4 L 10.4 19 L 9.2 19 Z M 12 8.4 L 13.2 8.4 L 13.2 19 L 12 19 Z"),
    ("fill", "M 15.6 8 C 21 8 21.4 9.4 21.4 13 C 21.4 16.6 21 18 15.6 18 L 15.6 16 C 19 16 19.2 15.4 19.2 13 C 19.2 10.6 19 10 15.6 10 Z"),
    ("fill", "M 3.4 2.4 C 5.8 1.4 8 3.6 10 2.4 C 12 1.2 14.4 2.6 16.2 2.4 L 16.2 4.4 L 3.4 4.4 Z"),
]

FOOD = [
    ("fill", "M 13.4 2.6 C 18.6 2.6 21.6 6.4 21.2 10.6 C 20.8 14.6 17.4 16.4 13.8 15.4 L 9.4 19.8 L 9.8 21 "
             "C 10 22.2 8.4 22.8 7.6 21.8 L 6.8 20.8 L 5.4 21.4 C 4.2 21.8 3.4 20.4 4.2 19.6 L 5.4 18.6 L 4.4 17.6 "
             "C 3.4 16.8 4.2 15.2 5.4 15.6 L 6.6 16 L 10.8 11.8 C 9.4 8.4 9.6 2.6 13.4 2.6 Z"),
]

NOTE = [
    ("fill", "M 5 2.4 L 15.2 2.4 L 19.6 6.8 L 19.6 21.6 L 5 21.6 Z M 14.4 3.6 L 14.4 7.6 L 18.4 7.6 Z "
             "M 7.6 10 L 17 10 L 17 11 L 7.6 11 Z M 7.6 13 L 17 13 L 17 14 L 7.6 14 Z M 7.6 16 L 14 16 L 14 17 L 7.6 17 Z"),
]

MAP = [
    ("fill", "M 2 5.4 L 8 3 L 16 5.4 L 22 3 L 22 18.6 L 16 21 L 8 18.6 L 2 21 Z "
             "M 7.4 4.6 L 8.6 4.6 L 8.6 19.4 L 7.4 19.4 Z M 15.4 6 L 16.6 6 L 16.6 20.4 L 15.4 20.4 Z"),
]

TOME = [
    ("fill", "M 4.6 3 L 18 3 C 18.8 3 19.4 3.6 19.4 4.4 L 19.4 21 L 6.4 21 C 5.4 21 4.6 20.2 4.6 19.2 Z "
             "M 12 6.6 L 13.2 10.2 L 16.8 10.2 L 13.9 12.4 L 15 16 L 12 13.8 L 9 16 L 10.1 12.4 L 7.2 10.2 L 10.8 10.2 Z "
             "M 6.4 18 L 17.6 18 L 17.6 19.4 L 6.4 19.4 Z"),
]

ORE = [
    ("fill", "M 6 8 L 11 4 L 17.6 5.6 L 21.4 11 L 19 18.4 L 12 21 L 4.4 18 L 2.6 12 Z "
             "M 11.4 6.6 L 16.2 8 L 15.4 11.6 L 11 12 Z M 6.6 13.6 L 9.8 14.2 L 9 17.4 L 6 16.6 Z"),
]

INGOT = [
    ("fill", "M 6.4 8 L 17.6 8 L 21.6 16 L 2.4 16 Z M 7.4 9.2 L 16.6 9.2 L 17.4 11 L 6.6 11 Z"),
    ("fill", "M 2.4 17.4 L 21.6 17.4 L 21.6 19.4 L 2.4 19.4 Z"),
]

PELT = [
    ("fill", "M 8 3 C 10 4.4 14 4.4 16 3 L 18.4 5.4 L 21.6 4.6 L 20.4 9 C 19 11 19 13.8 20.4 16 L 21.6 19.6 L 17.6 19 "
             "L 15 21.6 C 13 20.4 11 20.4 9 21.6 L 6.4 19 L 2.4 19.6 L 3.6 16 C 5 13.8 5 11 3.6 9 L 2.4 4.6 L 5.6 5.4 Z"),
]

SOULGEM = [
    ("fill", "M 12 1.8 L 18.6 9 L 12 22.2 L 5.4 9 Z M 12 5.4 L 15.4 9.6 L 12 17.4 L 8.6 9.6 Z"),
    ("fill", circle(12, 10.6, 1.8)),
]

SACK = [
    ("fill", "M 9 2.6 L 15 2.6 L 13.8 5.6 C 18.6 7.4 21 11.6 21 15.4 C 21 19.6 17.4 21.8 12 21.8 C 6.6 21.8 3 19.6 3 15.4 "
             "C 3 11.6 5.4 7.4 10.2 5.6 Z M 8.4 6.8 L 15.6 6.8 L 15.6 7.8 L 8.4 7.8 Z"),
]

# a bin with its lid, the slits cut through the body
JUNK = [
    ("fill", "M 9.4 2.4 L 14.6 2.4 L 14.6 5 L 9.4 5 Z M 10.8 3.6 L 13.2 3.6 L 13.2 5 L 10.8 5 Z"),
    ("fill", "M 3.6 5 L 20.4 5 L 20.4 7.4 L 3.6 7.4 Z"),
    ("fill", "M 5.4 8.6 L 18.6 8.6 L 17.2 21.8 L 6.8 21.8 Z "
             "M 8.4 10.8 L 9.8 10.8 L 10.2 19.6 L 8.8 19.6 Z M 11.3 10.8 L 12.7 10.8 L 12.7 19.6 L 11.3 19.6 Z "
             "M 14.2 10.8 L 15.6 10.8 L 15.2 19.6 L 13.8 19.6 Z"),
]

EYE = [
    ("fill", "M 1.6 12 C 5 6.2 19 6.2 22.4 12 C 19 17.8 5 17.8 1.6 12 Z " + circle(12, 12, 3.8) + " " + circle(12, 12, 1.9)),
]

EYE_SHUT = EYE + [("stroke", 2.2, "M 4 20.5 L 20 3.5")]

LENS = [
    ("stroke", 2.2, "M 15 15 L 21.4 21.4"),
    ("fill", circle(9.8, 9.8, 7.2) + " " + circle(9.8, 9.8, 5.2)),
]

# Signs of the item browser: its All tab and mod field, and the categories the panel had no sign for
BROWSER = [
    ("fill", "M 3 3 L 10.6 3 L 10.6 10.6 L 3 10.6 Z M 4.8 4.8 L 8.8 4.8 L 8.8 8.8 L 4.8 8.8 Z "
             "M 13.4 3 L 21 3 L 21 10.6 L 13.4 10.6 Z M 15.2 4.8 L 19.2 4.8 L 19.2 8.8 L 15.2 8.8 Z "
             "M 3 13.4 L 10.6 13.4 L 10.6 21 L 3 21 Z M 4.8 15.2 L 8.8 15.2 L 8.8 19.2 L 4.8 19.2 Z "
             "M 13.4 13.4 L 21 13.4 L 21 21 L 13.4 21 Z M 15.2 15.2 L 19.2 15.2 L 19.2 19.2 L 15.2 19.2 Z"),
]

# The eye that watches everything, the button of Know your Skyrim: a stern stare under a heavy brow that slants
# down to the nose
WATCH = [
    ("fill", "M 1.4 14.4 C 5 9 19 9 22.6 14.4 C 19 19.8 5 19.8 1.4 14.4 Z "
             "M 3.8 14.4 C 7 11 17 11 20.2 14.4 C 17 17.8 7 17.8 3.8 14.4 Z " + circle(12, 14.4, 3.6) + " " + circle(12, 14.4, 1.5)),
    ("fill", "M 2.2 9.8 C 8 6.2 15 4.4 22.2 5.4 L 22 7.9 C 15 7 8.6 8.4 3.2 11.8 Z"),
]

SCROLL = [
    ("fill", "M 5.6 2.6 L 18.4 2.6 C 20.4 2.6 20.4 6.2 18.4 6.2 L 5.6 6.2 C 3.6 6.2 3.6 2.6 5.6 2.6 Z"),
    ("fill", "M 6.4 6.6 L 17.6 6.6 L 17.6 17.4 L 6.4 17.4 Z M 8.4 9 L 15.6 9 L 15.6 10 L 8.4 10 Z "
             "M 8.4 11.6 L 15.6 11.6 L 15.6 12.6 L 8.4 12.6 Z M 8.4 14.2 L 13.6 14.2 L 13.6 15.2 L 8.4 15.2 Z"),
    ("fill", "M 5.6 17.8 L 18.4 17.8 C 20.4 17.8 20.4 21.4 18.4 21.4 L 5.6 21.4 C 3.6 21.4 3.6 17.8 5.6 17.8 Z"),
]

INGREDIENT = [
    ("stroke", 1.4, "M 12 22 C 12 16 12.6 10 16 3"),
    ("fill", "M 12.4 12 C 6 12.6 3.2 9 3.6 4.4 C 8.8 4 12.6 6.6 12.4 12 Z"),
    ("fill", "M 12.8 16.4 C 18.8 17.6 21.4 13.6 20.6 9.4 C 15.6 9.4 12.4 12 12.8 16.4 Z"),
]

KEY = [
    ("fill", circle(7, 12, 5) + " " + circle(7, 12, 2.6)),
    ("fill", "M 11.8 10.9 L 21.6 10.9 L 21.6 13.1 L 11.8 13.1 Z"),
    ("fill", "M 17.2 13.1 L 19 13.1 L 19 16.4 L 17.2 16.4 Z M 20 13.1 L 21.6 13.1 L 21.6 15.4 L 20 15.4 Z"),
]

CROSS = [("stroke", 2.2, "M 5 5 L 19 19"), ("stroke", 2.2, "M 19 5 L 5 19")]

PLUS = [("stroke", 2.2, "M 12 5 L 12 19"), ("stroke", 2.2, "M 5 12 L 19 12")]

HERO = [
    ("fill", circle(12, 7.2, 4.2)),
    ("fill", "M 3.6 21.4 C 3.6 15.8 7.4 12.9 12 12.9 C 16.6 12.9 20.4 15.8 20.4 21.4 Z"),
]

# a weight of the scales, the ring to lift it by over a block that widens down
WEIGHT = [
    ("fill", circle(12, 6, 3.4) + " " + circle(12, 6, 1.8)),
    ("fill", "M 7.6 9.4 L 16.4 9.4 L 20.4 21.4 L 3.6 21.4 Z"),
]

UNEQUIP = [
    ("stroke", 2.0, "M 4 12 L 20 12"),
    ("fill", "M 4 12 L 9.5 6.5 L 9.5 17.5 Z"),
]

DETAILS = [
    ("fill", circle(12, 6.2, 1.8)),
    ("fill", "M 10.4 10 L 13.6 10 L 13.6 21 L 10.4 21 Z"),
]

# Signs of the living things of the browser: a paw for animals, a skull for the undead, a horned head for the
# daedra, a wing for dragons, a cog for the dwarven automatons
def cog(x: float, y: float, outer: float, inner: float, teeth: int) -> str:
    """A closed gear outline, each tooth rising from the inner circle to the outer one"""
    step = 2 * math.pi / teeth
    points = []
    for tooth in range(teeth):
        for share, radius in ((0.0, inner), (0.15, outer), (0.45, outer), (0.6, inner)):
            angle = (tooth + share) * step
            points.append(f"{x + radius * math.cos(angle):.2f} {y + radius * math.sin(angle):.2f}")
    return "M " + " L ".join(points) + " Z"


PAW = [
    ("fill", "M 12 12.6 C 15.6 12.6 18.4 15.4 18.4 18.2 C 18.4 20.6 16.4 21.6 14.4 21.2 C 13.2 21 12.6 20.4 12 20.4 "
             "C 11.4 20.4 10.8 21 9.6 21.2 C 7.6 21.6 5.6 20.6 5.6 18.2 C 5.6 15.4 8.4 12.6 12 12.6 Z " +
             circle(5.2, 11, 2.1) + " " + circle(9.2, 6.6, 2.3) + " " + circle(14.8, 6.6, 2.3) + " " + circle(18.8, 11, 2.1)),
]

# everyone the browser knows, people and beasts: a person with a paw print at the side
LIVING = [
    ("fill", circle(7.6, 6.8, 3.4)),
    ("fill", "M 1.4 21.4 C 1.4 16.4 4.2 13.4 7.6 13.4 C 11 13.4 13.8 16.4 13.8 21.4 Z"),
    ("fill", "M 18.6 15.8 C 20.6 15.8 22.4 17.6 22.4 19.4 C 22.4 21 21.2 21.6 20 21.4 C 19.4 21.3 19 21 18.6 21 "
             "C 18.2 21 17.8 21.3 17.2 21.4 C 16 21.6 14.8 21 14.8 19.4 C 14.8 17.6 16.6 15.8 18.6 15.8 Z " +
             circle(14.9, 14.2, 1.2) + " " + circle(17.3, 12.3, 1.3) + " " + circle(19.9, 12.3, 1.3) + " " + circle(22.3, 14.2, 1.2)),
]

# kept alive by the game: a solid shield with a star cut out, apart from the shield of the Shields chip
WARD = [
    ("fill", "M 12 1.8 C 15 3.1 18 3.5 20.6 3.3 L 20.6 10 C 20.6 15.6 17 19.6 12 22.2 C 7 19.6 3.4 15.6 3.4 10 L 3.4 3.3 C 6 3.5 9 3.1 12 1.8 Z "
             "M 12 6.4 L 13.12 9.46 L 16.38 9.58 L 13.81 11.59 L 14.7 14.72 L 12 12.9 L 9.3 14.72 L 10.19 11.59 L 7.62 9.58 L 10.88 9.46 Z"),
]

# life, the sign of bringing back
HEART = [
    ("fill", "M 12 21 C 7.4 17.4 2.6 13.6 2.6 8.6 C 2.6 5.4 5 3.2 7.8 3.2 C 9.6 3.2 11.1 4.2 12 5.6 "
             "C 12.9 4.2 14.4 3.2 16.2 3.2 C 19 3.2 21.4 5.4 21.4 8.6 C 21.4 13.6 16.6 17.4 12 21 Z"),
]

SKULL = [
    ("fill", "M 12 2.4 C 17.4 2.4 20.6 6 20.6 10.6 C 20.6 13.4 19.2 15.2 17.6 16.2 L 17.6 19.2 L 6.4 19.2 L 6.4 16.2 "
             "C 4.8 15.2 3.4 13.4 3.4 10.6 C 3.4 6 6.6 2.4 12 2.4 Z " + circle(8.6, 11, 2.2) + " " + circle(15.4, 11, 2.2) + " "
             "M 12 13.4 L 13.2 15.6 L 10.8 15.6 Z"),
    ("fill", "M 7.6 20 L 16.4 20 L 16.4 21.8 L 7.6 21.8 Z"),
]

HORNS = [
    ("fill", "M 12 9 C 15.6 9 17.6 11.6 17.6 15 C 17.6 18.8 15 21.6 12 21.6 C 9 21.6 6.4 18.8 6.4 15 C 6.4 11.6 8.4 9 12 9 Z " +
             circle(9.8, 14.6, 1.2) + " " + circle(14.2, 14.6, 1.2)),
    ("fill", "M 7.6 11 C 4.6 9.8 3 6.6 3.6 2.4 C 5 5.2 7 6.6 9.4 9.2 Z"),
    ("fill", "M 16.4 11 C 19.4 9.8 21 6.6 20.4 2.4 C 19 5.2 17 6.6 14.6 9.2 Z"),
]

DRAGON = [
    ("fill", "M 2.4 4 C 7 4.6 11.4 7 14 10.6 C 16 8.4 18.8 7.6 21.6 7.8 C 19.6 9.4 18.6 11.6 18.6 14 C 16.4 13.4 14.6 14 13.6 15.6 "
             "C 12 14.4 10 14.4 8.6 15.4 C 8.6 13.6 7.4 12.2 5.6 11.8 C 6.2 9.6 4.8 6.4 2.4 4 Z"),
    ("stroke", 1.2, "M 13.6 15.6 L 12.6 21.6"),
]

COG = [
    ("fill", cog(12, 12, 10.2, 7.8, 8) + " " + circle(12, 12, 3.4)),
]

# the game's quest marker, an arrowhead pointing down with a notch on top
QUEST = [
    ("fill", "M 3.6 3 L 12 7.4 L 20.4 3 L 12 21.4 Z"),
]

PIN = [
    ("fill", "M 12 22 C 9 17.6 5.2 13.6 5.2 9.2 C 5.2 5.2 8.2 2.4 12 2.4 C 15.8 2.4 18.8 5.2 18.8 9.2 C 18.8 13.6 15 17.6 12 22 Z " +
             circle(12, 9.2, 2.8)),
]

# two people, the one behind peeks over the shoulder of the first
PEOPLE = [
    ("fill", circle(8.6, 7.8, 3.3)),
    ("fill", "M 2 21 C 2 16 4.8 13.2 8.6 13.2 C 12.4 13.2 15.2 16 15.2 21 Z"),
    ("fill", circle(16.2, 6.4, 2.8)),
    ("fill", "M 13.6 11.8 C 14.4 11.4 15.3 11.2 16.2 11.2 C 19.6 11.2 22 13.8 22 18.4 L 16.6 18.4 C 16.4 15.8 15.4 13.4 13.6 11.8 Z"),
]

# something that only pretends to be a thing: a ghost
GHOST = [
    ("fill", "M 5 21.4 L 5 10 C 5 5.6 8 2.6 12 2.6 C 16 2.6 19 5.6 19 10 L 19 21.4 L 16.6 19.4 L 14.2 21.4 L 12 19.4 "
             "L 9.8 21.4 L 7.4 19.4 Z " + circle(9.4, 10, 1.5) + " " + circle(14.6, 10, 1.5)),
]

SLIDERS = [
    ("stroke", 1.6, "M 3 6 L 21 6"),
    ("stroke", 1.6, "M 3 12 L 21 12"),
    ("stroke", 1.6, "M 3 18 L 21 18"),
    ("fill", circle(8, 6, 2.4)),
    ("fill", circle(16, 12, 2.4)),
    ("fill", circle(10, 18, 2.4)),
]

# name, layers of parts with a turn in degrees around the middle and a scale around the middle
ICONS = {
    "sword": (SWORD, 45.0, 1.12),
    "shield": (SHIELD, 0.0, 1.0),
    "arrows": (ARROW, 40.0, 1.1),
    "amulet": (AMULET, 0.0, 1.0),
    "ring": (RING, 0.0, 1.0),
    "helmet": (HELMET, 0.0, 1.0),
    "cuirass": (CUIRASS, 0.0, 1.0),
    "gauntlets": (GAUNTLET, 0.0, 1.0),
    "boots": (BOOT, 0.0, 1.0),
    "cloak": (CLOAK, 0.0, 1.0),
    "battleaxe": (BATTLEAXE, 0.0, 1.0),
    "bow": (BOW, 45.0, 1.05),
    "staff": (STAFF, 30.0, 1.0),
    "jerkin": (JERKIN, 0.0, 1.0),
    "plate": (PLATE, 0.0, 1.0),
    "robe": (ROBE, 0.0, 1.0),
    "star": (STAR, 0.0, 1.0),
    "drop": (DROP, 0.0, 1.0),
    "transfer": (TRANSFER, 0.0, 1.0),
    "coin": (COIN, 0.0, 1.0),
    "potion": (POTION, 0.0, 1.0),
    "book": (BOOK, 0.0, 1.0),
    "gem": (GEM, 0.0, 1.0),
    "amount": (AMOUNT, 0.0, 1.0),
    "poison": (POISON, 0.0, 1.0),
    "mug": (MUG, 0.0, 1.0),
    "food": (FOOD, 0.0, 1.0),
    "note": (NOTE, 0.0, 1.0),
    "map": (MAP, 0.0, 1.0),
    "tome": (TOME, 0.0, 1.0),
    "ore": (ORE, 0.0, 1.0),
    "ingot": (INGOT, 0.0, 1.0),
    "pelt": (PELT, 0.0, 1.0),
    "soulgem": (SOULGEM, 0.0, 1.0),
    "sack": (SACK, 0.0, 1.0),
    "junk": (JUNK, 0.0, 1.0),
    "eye": (EYE, 0.0, 1.0),
    "eyeShut": (EYE_SHUT, 0.0, 1.0),
    "lens": (LENS, 0.0, 1.0),
    "browser": (BROWSER, 0.0, 1.0),
    "watch": (WATCH, 0.0, 1.0),
    "scroll": (SCROLL, 0.0, 1.0),
    "ingredient": (INGREDIENT, 0.0, 1.0),
    "key": (KEY, 0.0, 1.0),
    "cross": (CROSS, 0.0, 1.0),
    "plus": (PLUS, 0.0, 1.0),
    "hero": (HERO, 0.0, 1.0),
    "weight": (WEIGHT, 0.0, 1.0),
    "unequip": (UNEQUIP, 0.0, 1.0),
    "details": (DETAILS, 0.0, 1.0),
    "paw": (PAW, 0.0, 1.0),
    "living": (LIVING, 0.0, 1.0),
    "heart": (HEART, 0.0, 1.0),
    "ward": (WARD, 0.0, 1.0),
    "skull": (SKULL, 0.0, 1.0),
    "horns": (HORNS, 0.0, 1.0),
    "dragon": (DRAGON, 0.0, 1.0),
    "cog": (COG, 0.0, 1.0),
    "quest": (QUEST, 0.0, 1.0),
    "pin": (PIN, 0.0, 1.0),
    "people": (PEOPLE, 0.0, 1.0),
    "ghost": (GHOST, 0.0, 1.0),
    "sliders": (SLIDERS, 0.0, 1.0),
}


def tokens(path: str) -> list[str]:
    return re.findall(r"[MLHVQCZ]|-?\d+(?:\.\d+)?", path)


def parse(path: str) -> list[list[tuple]]:
    """Contours of a path, each a list of (MOVE, x, y), (LINE, x, y), (CURVE, cx, cy, x, y) in quadratic curves"""
    found, contour = [], []
    words = tokens(path)
    at, x, y, start = 0, 0.0, 0.0, (0.0, 0.0)

    def take(count: int) -> list[float]:
        nonlocal at
        values = [float(word) for word in words[at:at + count]]
        at += count
        return values

    while at < len(words):
        command = words[at]
        at += 1
        if command == "M":
            if contour:
                found.append(contour)
            x, y = take(2)
            start = (x, y)
            contour = [(MOVE, x, y)]
        elif command == "L":
            x, y = take(2)
            contour.append((LINE, x, y))
        elif command == "H":
            (x,) = take(1)
            contour.append((LINE, x, y))
        elif command == "V":
            (y,) = take(1)
            contour.append((LINE, x, y))
        elif command == "Q":
            cx, cy, x, y = take(4)
            contour.append((CURVE, cx, cy, x, y))
        elif command == "C":
            # a cubic is split in two halves, each close enough to one quadratic curve
            c1x, c1y, c2x, c2y, ex, ey = take(6)
            p0, p1, p2, p3 = (x, y), (c1x, c1y), (c2x, c2y), (ex, ey)
            mid = lambda a, b: ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
            m01, m12, m23 = mid(p0, p1), mid(p1, p2), mid(p2, p3)
            m012, m123 = mid(m01, m12), mid(m12, m23)
            half = mid(m012, m123)
            for a, b, c, d in ((p0, m01, m012, half), (half, m123, m23, p3)):
                control = ((3 * (b[0] + c[0]) - a[0] - d[0]) / 4, (3 * (b[1] + c[1]) - a[1] - d[1]) / 4)
                contour.append((CURVE, control[0], control[1], d[0], d[1]))
            x, y = ex, ey
        elif command == "Z":
            if (x, y) != start:
                contour.append((LINE, start[0], start[1]))
            x, y = start
    if contour:
        found.append(contour)
    return found


def placed(contours: list[list[tuple]], turn: float, scale: float) -> list[list[tuple]]:
    """The contours turned and scaled around the middle of the box"""
    angle = math.radians(turn)
    cos, sin = math.cos(angle), math.sin(angle)
    middle = BOX / 2

    def point(px: float, py: float) -> tuple[float, float]:
        dx, dy = (px - middle) * scale, (py - middle) * scale
        return middle + dx * cos - dy * sin, middle + dx * sin + dy * cos

    result = []
    for contour in contours:
        moved = []
        for step in contour:
            if step[0] == CURVE:
                cx, cy = point(step[1], step[2])
                ex, ey = point(step[3], step[4])
                moved.append((CURVE, cx, cy, ex, ey))
            else:
                px, py = point(step[1], step[2])
                moved.append((step[0], px, py))
        result.append(moved)
    return result


def icon(name: str) -> list[tuple]:
    """Parts of an icon ready to draw: ("fill", contours) or ("stroke", width, contours)"""
    layers = ICONS[name] if isinstance(ICONS[name], list) else [ICONS[name]]
    ready = []
    for parts, turn, scale in layers:
        for part in parts:
            if part[0] == "fill":
                ready.append(("fill", placed(parse(part[1]), turn, scale)))
            else:
                ready.append(("stroke", part[1] * scale, placed(parse(part[2]), turn, scale)))
    return ready


def flatten(contour: list[tuple], size: float) -> list[tuple[float, float]]:
    points, last = [], (0.0, 0.0)
    for step in contour:
        if step[0] == CURVE:
            for t in (i / 8 for i in range(1, 9)):
                u = 1 - t
                points.append(((u * u * last[0] + 2 * u * t * step[1] + t * t * step[3]) * size,
                               (u * u * last[1] + 2 * u * t * step[2] + t * t * step[4]) * size))
            last = (step[3], step[4])
        else:
            last = (step[1], step[2])
            points.append((last[0] * size, last[1] * size))
    return points


def render(name: str, pixels: int) -> Image.Image:
    """One icon white on transparent, the fill even-odd as the menu draws it"""
    unit = pixels / BOX
    shape = Image.new("L", (pixels, pixels), 0)
    for part in icon(name):
        if part[0] == "fill":
            area = Image.new("L", (pixels, pixels), 0)
            for contour in part[1]:
                single = Image.new("L", (pixels, pixels), 0)
                ImageDraw.Draw(single).polygon(flatten(contour, unit), fill=255)
                area = ImageChops.logical_xor(area.convert("1"), single.convert("1")).convert("L")
            shape = ImageChops.lighter(shape, area)
        else:
            lines = Image.new("L", (pixels, pixels), 0)
            for contour in part[2]:
                ImageDraw.Draw(lines).line(flatten(contour, unit), fill=255, width=max(1, round(part[1] * unit)))
            shape = ImageChops.lighter(shape, lines)
    picture = Image.new("RGBA", (pixels, pixels), (230, 220, 200, 0))
    picture.putalpha(shape)
    return picture


def sheet(pixels: int = 96) -> Path:
    names = list(ICONS)
    columns = 8
    rows = (len(names) + columns - 1) // columns
    canvas = Image.new("RGBA", (columns * (pixels + 16) + 16, rows * (pixels + 16) + 16), (24, 24, 24, 255))
    for index, name in enumerate(names):
        drawn = render(name, pixels * 4).resize((pixels, pixels), Image.LANCZOS)
        x = 16 + (index % columns) * (pixels + 16)
        y = 16 + (index // columns) * (pixels + 16)
        canvas.alpha_composite(drawn, (x, y))
    SHEET.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(SHEET)
    return SHEET


def tinted(name: str, pixels: int, colour: tuple[int, int, int]) -> Image.Image:
    drawn = render(name, pixels * 4).resize((pixels, pixels), Image.LANCZOS)
    paint = Image.new("RGBA", drawn.size, colour + (255,))
    paint.putalpha(drawn.getchannel("A"))
    return paint


def bar_button(name: str, scale: float, lit: bool) -> Image.Image:
    """The browser's sign-only bar button as the menu draws it, measured on the 1280 by 720 stage"""
    wide, high, x, y, sign = BAR_BUTTON
    button = Image.new("RGBA", (round(wide * scale), round(high * scale)), (0, 0, 0, 0))
    ImageDraw.Draw(button).rounded_rectangle((0, 0, button.width - 1, button.height - 1), radius=round(5 * scale),
                                             fill=(0, 0, 0, 158 if lit else 107), outline=GOLD + ((255 if lit else 191),),
                                             width=max(1, round(scale)))
    button.alpha_composite(tinted(name, round(sign * scale), GOLD if lit else PARCHMENT), (round(x * scale), round(y * scale)))
    return button


def choice(names: list[str], zoom: int = 5) -> Path:
    """Candidates side by side: large, then the bar button at 1080p in both states, then the same button zoomed"""
    rows = []
    for name in names:
        parts = [tinted(name, 128, PARCHMENT)]
        for lit in (False, True):
            parts.append(bar_button(name, 1.5, lit))
        for lit in (False, True):
            parts.append(bar_button(name, 1.5 * zoom, lit))
        rows.append(parts)
    gap = 24
    wide = gap + sum(part.width + gap for part in rows[0])
    high = gap + sum(max(part.height for part in row) + gap for row in rows)
    canvas = Image.new("RGBA", (wide, high), (28, 26, 24, 255))
    y = gap
    for row in rows:
        tall = max(part.height for part in row)
        x = gap
        for part in row:
            canvas.alpha_composite(part, (x, y + (tall - part.height) // 2))
            x += part.width + gap
        y += tall + gap
    CHOICE.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(CHOICE)
    return CHOICE


def numbers(values: list[float]) -> str:
    return ",".join(f"{value:.2f}".rstrip("0").rstrip(".") for value in values)


def script() -> str:
    """Equipment.ICONS for the menu: per icon a list of parts, a part is [width, n, n, ...] with width 0 for
    a fill, the numbers are command, x, y for a move or a line and command, cx, cy, x, y for a curve"""
    lines = ["Equipment.ICONS = {};"]
    for name in ICONS:
        parts = []
        for part in icon(name):
            width, contours = (0.0, part[1]) if part[0] == "fill" else (part[1], part[2])
            flat = [width]
            for contour in contours:
                for step in contour:
                    flat.extend(step)
            parts.append("[" + numbers(flat) + "]")
        lines.append(f"Equipment.ICONS.{name} = [{','.join(parts)}];")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    import sys
    print(choice(sys.argv[2:]) if sys.argv[1:2] == ["--choose"] else sheet())
