"""The words of Inventory Reforged in translation files the game reads in its own language

The panel asks for every word it shows with Equipment.say("English words") and the game looks up
"$IR_English words" in Interface/Translations/InventoryReforged_<LANGUAGE>.txt. The menus get the same keys:
names, headers, pages and choices as "$IR_<text>", a help line as "$IR_help <id of its setting>".
menu/config.json stays in English and the build writes a keyed copy of it for both menus.
The package carries the English words under every language of the game, so a menu never shows a bare key.
A translator replaces the right side of the lines in the file of their language.
Know your Skyrim words its browser the same way, KnowYourSkyrim.say and "$KYSK_" in KnowYourSkyrim_<LANGUAGE>.txt
"""

import json
import re
from pathlib import Path

PREFIX = "$IR_"
STEM = "InventoryReforged"
FOLDER = Path("Interface") / "Translations"
LANGUAGES = ("ENGLISH", "CZECH", "FRENCH", "GERMAN", "ITALIAN", "JAPANESE", "POLISH", "RUSSIAN", "SPANISH", "CHINESE")

# the words of a subcategory SkyUI has no translation of
FALLBACK = re.compile(r'fallback:"([^"]*)"')
# the words the plugin draws itself, in SKSE Menu Framework
PLUGIN_SAID = re.compile(r'Translations::Word\("\$IR_((?:[^"\\]|\\.)*)"\)')


def panel_words(parts: list[str], name: str = "Equipment") -> list[str]:
    """Every literal the parts hand to <name>.say(), in the order they first appear"""
    said = re.compile(rf'\b{name}\.say(?:Pieces)?\("((?:[^"\\]|\\.)*)"')
    found: dict[str, None] = {}
    for text in parts:
        for match in said.finditer(text):
            found.setdefault(match.group(1).replace('\\"', '"'), None)
        for match in FALLBACK.finditer(text):
            found.setdefault(match.group(1), None)
    return list(found)


def plugin_words(sources: list[str]) -> list[str]:
    """Every literal key the plugin's sources hand to Translations::Word, without its $IR_"""
    found: dict[str, None] = {}
    for text in sources:
        for match in PLUGIN_SAID.finditer(text):
            found.setdefault(match.group(1).replace('\\"', '"'), None)
    return list(found)


def keyed_pages(pages: dict, prefix: str = PREFIX) -> tuple[dict, dict[str, str]]:
    """The pages with keys in place of their words, and the English of every key"""
    keyed = json.loads(json.dumps(pages))
    english: dict[str, str] = {}

    def key(words: str) -> str:
        made = prefix + words
        english[made] = words
        return made

    for page in keyed["pages"]:
        page["pageDisplayName"] = key(page["pageDisplayName"])
        for item in page["content"]:
            if item.get("text"):
                item["text"] = key(item["text"])
            if item.get("help"):
                made = f"{prefix}help {item.get('id', item['text'])}"
                english[made] = item["help"]
                item["help"] = made
            options = item.get("valueOptions", {})
            if "options" in options:
                options["options"] = [key(choice) for choice in options["options"]]
    return keyed, english


def lines(words: list[str], menu: dict[str, str], prefix: str = PREFIX) -> list[str]:
    entries = {prefix + one: one for one in words}
    entries.update(menu)
    for made, text in entries.items():
        if "\t" in text or "\n" in text or "{" in made or "}" in made:
            raise ValueError(f"a key or its words can't hold a tab, a line break or braces: {made}")
    return [f"{made}\t{text}" for made, text in entries.items()]


def write(target: Path, entries: list[str], languages: tuple[str, ...] = LANGUAGES, stem: str = STEM) -> list[Path]:
    """The game reads these files as UTF-16 with a byte order mark, a line a key"""
    folder = target / FOLDER
    folder.mkdir(parents=True, exist_ok=True)
    body = "﻿" + "\r\n".join(entries) + "\r\n"
    written = []
    for language in languages:
        file = folder / f"{stem}_{language}.txt"
        file.write_bytes(body.encode("utf-16-le"))
        written.append(file)
    return written


def pseudo(package: Path, target: Path, stem: str = STEM) -> Path:
    """A layer for the stand whose English file wraps every word in «», a word on screen without them never
    went through the file"""
    english = (package / FOLDER / f"{stem}_ENGLISH.txt").read_bytes().decode("utf-16-le").lstrip("﻿")
    wrapped = []
    for line in english.splitlines():
        made, text = line.split("\t", 1)
        wrapped.append(f"{made}\t«{text}»")
    return write(target, wrapped, ("ENGLISH",), stem)[0]


if __name__ == "__main__":
    # python -m interface.equipment.translations <folder> [kysk]
    import sys
    if sys.argv[2:] == ["kysk"]:
        from interface.know_your_skyrim import build as browser
        print(pseudo(browser.PACKAGE, Path(sys.argv[1]), browser.STEM))
    else:
        from interface.equipment import build
        print(pseudo(build.PACKAGE, Path(sys.argv[1])))
