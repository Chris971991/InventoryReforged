"""A menu in SkyUI's own Mod Configuration made from MCM Helper's pages, so the mod needs no MCM Helper

The pages stay in the config.json form MCM Helper reads (pageDisplayName, content of header, toggle, enum, slider,
keymap, color and text with a CallFunction action). script() writes the Papyrus of a menu that extends SKI_ConfigBase and
shows those pages. It reads and sets every value through two functions the mod's own plugin gives to scripts,
    Float GetSetting(String id) and SetSetting(String id, Float value) of the native script,
with ids as the pages name them, "bModels:Slots". The plugin keeps them where it likes, the menu doesn't know.
A page's groups (a header and what follows it) fill the left column and go on in the right one at about half.
The quest and plugin of interface.mcm carry the script as they carried the one for MCM Helper
"""

import json
from dataclasses import dataclass, field
from pathlib import Path

KINDS = ("toggle", "enum", "slider", "keymap", "color", "text")
FILE_HEADER = """Scriptname {script} extends SKI_ConfigBase
{{The menu of {shown} in SkyUI's own Mod Configuration. interface.skyui_menu writes it from the pages in
{pages}, {native} reads and sets the values}}

; the settings of the pages, one place each, as the build found them
String[] ids
String[] kinds
String[] names
String[] helps
Float[] defaults
Float[] lows
Float[] highs
Float[] steps
String[] formats
Int[] options
"""


@dataclass
class Control:
    kind: str
    name: str
    help: str
    id: str = ""
    choices: list[str] = field(default_factory=list)
    low: float = 0.0
    high: float = 1.0
    step: float = 1.0
    format: str = "{0}"
    call: str = ""


@dataclass
class Group:
    header: str
    controls: list[Control]


def quoted(text: str) -> str:
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def number(value: float) -> str:
    return repr(float(value))


def pages_of(pages: Path) -> list[tuple[str, list[Group]]]:
    """Every page with its groups. Controls before the first header make a group with no header"""
    told = json.loads(pages.read_text(encoding="utf-8"))
    found = []
    for page in told["pages"]:
        groups: list[Group] = []
        for item in page["content"]:
            if item["type"] == "header":
                groups.append(Group(item["text"], []))
                continue
            if item["type"] not in KINDS:
                raise ValueError(f"the menu has no way to show a control of type {item['type']}")
            if not groups:
                groups.append(Group("", []))
            options = item.get("valueOptions", {})
            control = Control(item["type"], item["text"], item.get("help", ""), item.get("id", ""))
            if item["type"] == "enum":
                control.choices = list(options["options"])
            elif item["type"] == "slider":
                control.low, control.high, control.step = options["min"], options["max"], options["step"]
                control.format = options.get("formatString", "{0}")
            elif item["type"] == "text":
                action = item.get("action", {})
                if action.get("type") != "CallFunction":
                    raise ValueError(f"the text {item['text']} does nothing the menu knows")
                control.call = action["function"]
                control.id = "action:" + control.call
            groups[-1].controls.append(control)
        found.append((page["pageDisplayName"], groups))
    return found


def columns(groups: list[Group]) -> tuple[list[Group], list[Group]]:
    """The groups of the left column and of the right one, the left holding about half of the rows"""
    rows = [len(group.controls) + (1 if group.header else 0) for group in groups]
    left, filled = [], 0
    for group, high in zip(groups, rows):
        if left and filled + high / 2 > sum(rows) / 2:
            break
        left.append(group)
        filled += high
    return left, groups[len(left):]


def describe(controls: list[Control], defaults: dict[str, float]) -> list[str]:
    count = len(controls)
    lines = ["Function Describe()"]
    for name, kind in (("ids", "String"), ("kinds", "String"), ("names", "String"), ("helps", "String"), ("defaults", "Float"),
                       ("lows", "Float"), ("highs", "Float"), ("steps", "Float"), ("formats", "String"), ("options", "Int")):
        lines.append(f"\t{name} = new {kind}[{count}]")
    for place, control in enumerate(controls):
        lines += [
            f"\tids[{place}] = {quoted(control.id)}",
            f"\tkinds[{place}] = {quoted(control.kind)}",
            f"\tnames[{place}] = {quoted(control.name)}",
            f"\thelps[{place}] = {quoted(control.help)}",
            f"\tdefaults[{place}] = {number(defaults.get(control.id, 0.0))}",
        ]
        if control.kind == "slider":
            lines += [f"\tlows[{place}] = {number(control.low)}", f"\thighs[{place}] = {number(control.high)}",
                      f"\tsteps[{place}] = {number(control.step)}", f"\tformats[{place}] = {quoted(control.format)}"]
    lines.append("EndFunction")
    return lines


def shown(control: Control, place: int, native: str) -> str:
    """The line that puts a control on its page and keeps its option"""
    value = f"{native}.GetSetting(ids[{place}])"
    if control.kind == "toggle":
        made = f"AddToggleOption(names[{place}], {value} != 0.0, OPTION_FLAG_NONE)"
    elif control.kind == "enum":
        made = f"AddMenuOption(names[{place}], Choice({place}, {value} as Int), OPTION_FLAG_NONE)"
    elif control.kind == "slider":
        made = f"AddSliderOption(names[{place}], {value}, formats[{place}], OPTION_FLAG_NONE)"
    elif control.kind == "keymap":
        made = f"AddKeyMapOption(names[{place}], KeyOf({value}), OPTION_FLAG_WITH_UNMAP)"
    elif control.kind == "color":
        made = f"AddColorOption(names[{place}], {value} as Int, OPTION_FLAG_NONE)"
    else:
        made = f'AddTextOption(names[{place}], "", OPTION_FLAG_NONE)'
    return f"\t\toptions[{place}] = {made}"


def script(pages: Path, name: str, shown_name: str, native: str, defaults: dict[str, float]) -> str:
    """The Papyrus source of the menu"""
    laid = pages_of(pages)
    controls = [control for _, groups in laid for group in groups for control in group.controls]
    place = {id(control): number for number, control in enumerate(controls)}
    lines = FILE_HEADER.format(script=name, shown=shown_name, pages=pages.name, native=native).splitlines()

    lines += ["", "Int Function GetVersion()", "\tReturn 2", "EndFunction", "", "Function OnConfigInit()",
              f"\tPages = new String[{len(laid)}]"]
    lines += [f"\tPages[{number}] = {quoted(title)}" for number, (title, _) in enumerate(laid)]
    lines += ["EndFunction", "", "; an older menu of the same quest gets the pages of this one",
              "Function OnVersionUpdate(Int version)", "\tOnConfigInit()", "EndFunction", ""]
    lines += describe(controls, defaults)

    lines += ["", "Function OnPageReset(String page)", "\tDescribe()", "\tSetCursorFillMode(TOP_TO_BOTTOM)"]
    for number, (title, groups) in enumerate(laid):
        lines.append(f"\t{'If' if number == 0 else 'ElseIf'} page == {quoted(title)}")
        for side, column in enumerate(columns(groups)):
            if side == 1 and column:
                lines.append("\t\tSetCursorPosition(1)")
            for group in column:
                if group.header:
                    lines.append(f"\t\tAddHeaderOption({quoted(group.header)}, OPTION_FLAG_NONE)")
                lines += [shown(control, place[id(control)], native) for control in group.controls]
    lines += ["\tEndIf", "EndFunction"]

    enums = [(place[id(control)], control) for control in controls if control.kind == "enum"]
    lines += ["", "; the words of a choice, an unknown number gives the first", "String[] Function Choices(Int place)"]
    for number, (spot, control) in enumerate(enums):
        lines += [f"\t{'If' if number == 0 else 'ElseIf'} place == {spot}", f"\t\tString[] words = new String[{len(control.choices)}]"]
        lines += [f"\t\twords[{index}] = {quoted(choice)}" for index, choice in enumerate(control.choices)]
        lines.append("\t\tReturn words")
    if enums:
        lines.append("\tEndIf")
    lines += ["\tString[] nothing = new String[1]", "\tReturn nothing", "EndFunction"]

    actions = [control for control in controls if control.kind == "text"]
    lines += ["", "Function Act(String call)"]
    for number, control in enumerate(actions):
        lines += [f"\t{'If' if number == 0 else 'ElseIf'} call == {quoted(control.id)}", f"\t\t{control.call}()"]
    if actions:
        lines.append("\tEndIf")
    lines += ["EndFunction", ""]
    lines += BEHAVIOUR.format(native=native).splitlines()
    return "\n".join(lines) + "\n"


# what every menu made here does with its controls, the same for all of them
BEHAVIOUR = """String Function Choice(Int place, Int index)
	String[] words = Choices(place)
	If index < 0 || index >= words.Length
		index = 0
	EndIf
	Return words[index]
EndFunction

; the plugin keeps 0 for no key, the menu shows that as -1
Int Function KeyOf(Float value)
	If value <= 0.0
		Return -1
	EndIf
	Return value as Int
EndFunction

Int Function Place(Int option)
	If options == None
		Return -1
	EndIf
	Int i = 0
	While i < options.Length
		If options[i] == option
			Return i
		EndIf
		i += 1
	EndWhile
	Return -1
EndFunction

Function OnOptionHighlight(Int option)
	Int i = Place(option)
	If i >= 0
		SetInfoText(helps[i])
	EndIf
EndFunction

Function OnOptionSelect(Int option)
	Int i = Place(option)
	If i < 0
		Return
	EndIf
	If kinds[i] == "toggle"
		Bool on = {native}.GetSetting(ids[i]) == 0.0
		If on
			{native}.SetSetting(ids[i], 1.0)
		Else
			{native}.SetSetting(ids[i], 0.0)
		EndIf
		SetToggleOptionValue(option, on, False)
	ElseIf kinds[i] == "text"
		Act(ids[i])
		SetTextOptionValue(option, "done", False)
	EndIf
EndFunction

Function OnOptionSliderOpen(Int option)
	Int i = Place(option)
	If i >= 0
		SetSliderDialogStartValue({native}.GetSetting(ids[i]))
		SetSliderDialogDefaultValue(defaults[i])
		SetSliderDialogRange(lows[i], highs[i])
		SetSliderDialogInterval(steps[i])
	EndIf
EndFunction

Function OnOptionSliderAccept(Int option, Float value)
	Int i = Place(option)
	If i >= 0
		{native}.SetSetting(ids[i], value)
		SetSliderOptionValue(option, value, formats[i], False)
	EndIf
EndFunction

Function OnOptionMenuOpen(Int option)
	Int i = Place(option)
	If i >= 0
		SetMenuDialogOptions(Choices(i))
		SetMenuDialogStartIndex({native}.GetSetting(ids[i]) as Int)
		SetMenuDialogDefaultIndex(defaults[i] as Int)
	EndIf
EndFunction

Function OnOptionMenuAccept(Int option, Int index)
	Int i = Place(option)
	If i >= 0 && index >= 0
		{native}.SetSetting(ids[i], index as Float)
		SetMenuOptionValue(option, Choice(i, index), False)
	EndIf
EndFunction

; a colour 0xRRGGBB is kept as its number, a Float holds every one of them exactly
Function OnOptionColorOpen(Int option)
	Int i = Place(option)
	If i >= 0
		SetColorDialogStartColor({native}.GetSetting(ids[i]) as Int)
		SetColorDialogDefaultColor(defaults[i] as Int)
	EndIf
EndFunction

Function OnOptionColorAccept(Int option, Int color)
	Int i = Place(option)
	If i >= 0
		{native}.SetSetting(ids[i], color as Float)
		SetColorOptionValue(option, color, False)
	EndIf
EndFunction

; a key another control already holds is taken only after the player agrees
Function OnOptionKeyMapChange(Int option, Int keyCode, String conflictControl, String conflictName)
	Int i = Place(option)
	If i < 0
		Return
	EndIf
	If conflictControl != "" && keyCode > 0
		String holder = conflictControl
		If conflictName != ""
			holder = conflictControl + " (" + conflictName + ")"
		EndIf
		If !ShowMessage("This key is already used by " + holder + ". Use it here too?", True, "$Accept", "$Cancel")
			Return
		EndIf
	EndIf
	If keyCode > 0
		{native}.SetSetting(ids[i], keyCode as Float)
	Else
		{native}.SetSetting(ids[i], 0.0)
	EndIf
	SetKeyMapOptionValue(option, keyCode, False)
EndFunction

Function OnOptionDefault(Int option)
	Int i = Place(option)
	If i >= 0 && kinds[i] != "text"
		{native}.SetSetting(ids[i], defaults[i])
		ForcePageReset()
	EndIf
EndFunction
"""
