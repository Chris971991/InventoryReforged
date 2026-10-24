"""Add a script to the first frame of a Flash menu file and prove nothing else changed

The classes of a menu stay as compiled by its author. The added script hooks
into them when the menu starts, so the only tag allowed to differ between the
original and the result is the script of the first frame
"""

import shutil
import struct
import subprocess
import zlib
from pathlib import Path

from common.paths import TOOLS

FFDEC = TOOLS / "ffdec" / "app" / "ffdec.jar"
FRAME_SCRIPT = Path("scripts") / "frame_1" / "DoAction.as"
FRAME_SCRIPT_TAG = 12
HEADER_SIZE = 8
FRAME_RATE_AND_COUNT_SIZE = 4
LONG_TAG = 0x3F


def ffdec(*arguments) -> None:
    subprocess.run(["java", "-jar", str(FFDEC), *map(str, arguments)], check=True, capture_output=True)


def tags(file: Path) -> list[tuple[int, bytes]]:
    data = file.read_bytes()
    body = zlib.decompress(data[HEADER_SIZE:]) if data[:3] == b"CWS" else data[HEADER_SIZE:]

    frame_size_bits = body[0] >> 3
    offset = (5 + 4 * frame_size_bits + 7) // 8 + FRAME_RATE_AND_COUNT_SIZE
    found = []
    while offset < len(body):
        code_and_length = struct.unpack_from("<H", body, offset)[0]
        code, length = code_and_length >> 6, code_and_length & LONG_TAG
        offset += 2
        if length == LONG_TAG:
            length = struct.unpack_from("<I", body, offset)[0]
            offset += 4
        found.append((code, body[offset:offset + length]))
        offset += length
    return found


FILE_ATTRIBUTES_TAG = 69
BACKGROUND_TAG = 9
SHOW_FRAME_TAG = 1
END_TAG = 0
ACTION_END = b"\x00"


def tag(code: int, body: bytes) -> bytes:
    if len(body) < LONG_TAG:
        return struct.pack("<H", code << 6 | len(body)) + body
    return struct.pack("<HI", code << 6 | LONG_TAG, len(body)) + body


def stage_rect(wide_twips: int, high_twips: int) -> bytes:
    """A RECT of the SWF header: five bits of field size, then four fields from 0, 0"""
    bits = 16
    values = [0, wide_twips, 0, high_twips]
    stream = format(bits, "05b") + "".join(format(value, f"0{bits}b") for value in values)
    stream += "0" * (-len(stream) % 8)
    return int(stream, 2).to_bytes(len(stream) // 8, "big")


def empty_movie(actions: int, version: int = 8) -> bytes:
    """A movie of one frame that holds this many empty frame scripts and nothing else"""
    body = stage_rect(25600, 14400) + struct.pack("<HH", 30 << 8, 1)
    body += tag(FILE_ATTRIBUTES_TAG, struct.pack("<I", 0))
    body += tag(BACKGROUND_TAG, b"\x00\x00\x00")
    for _ in range(actions):
        body += tag(FRAME_SCRIPT_TAG, ACTION_END)
    body += tag(SHOW_FRAME_TAG, b"") + tag(END_TAG, b"")
    return b"FWS" + bytes([version]) + struct.pack("<I", HEADER_SIZE + len(body)) + body


def script_movie(result: Path, parts: list[str], work: Path) -> None:
    """A movie of its own whose first frame runs the parts one after another, each a frame script of its own
    so no single script grows past what one may hold"""
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    blank = work / "blank.swf"
    blank.write_bytes(empty_movie(len(parts)))
    ffdec("-export", "script", work / "blank", blank)
    found = sorted((work / "blank" / "scripts" / "frame_1").glob("DoAction*.as"), key=lambda path: (len(path.name), path.name))
    if len(found) != len(parts):
        raise ValueError(f"the blank movie shows {len(found)} frame scripts for {len(parts)} parts")
    target = work / "patch" / "scripts" / "frame_1"
    target.mkdir(parents=True)
    for file, part in zip(found, parts):
        (target / file.name).write_text(part, encoding="utf-8")
    result.parent.mkdir(parents=True, exist_ok=True)
    ffdec("-onerror", "abort", "-importScript", blank, result, work / "patch")
    actions = [body for code, body in tags(result) if code == FRAME_SCRIPT_TAG]
    if len(actions) != len(parts) or any(body == ACTION_END for body in actions):
        raise ValueError(f"{result.name}: not every part was compiled into the movie")


def add_frame_script(original: Path, result: Path, added: str, work: Path) -> None:
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    ffdec("-export", "script", work / "original", original)

    frame = (work / "original" / FRAME_SCRIPT).read_text(encoding="utf-8")
    patch = work / "patch" / FRAME_SCRIPT
    patch.parent.mkdir(parents=True)
    patch.write_text(frame + "\n" + added, encoding="utf-8")

    result.parent.mkdir(parents=True, exist_ok=True)
    ffdec("-onerror", "abort", "-importScript", original, result, work / "patch")

    before, after = tags(original), tags(result)
    changed = [after[index][0] for index in range(len(after)) if index >= len(before) or before[index] != after[index]]
    if len(before) != len(after) or changed != [FRAME_SCRIPT_TAG]:
        raise ValueError(f"{result.name}: more than the first frame script differs from the original, tags {changed}")
