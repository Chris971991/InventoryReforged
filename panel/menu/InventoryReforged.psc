Scriptname InventoryReforged Native Hidden
{Functions the plugin InventoryReforged.dll gives to scripts}

; ---- The mod's own menu. Not for other mods, these may change in any version ----

; every hidden slot is drawn on the player again, the plugin keeps the hidden slots in the save
Function ShowAllHidden() Global Native

; a setting by its id as the menu names it, "bModels:Slots" is bModels in [Slots] of the ini
Float Function GetSetting(String id) Global Native

; written into MCM/Settings/InventoryReforged.ini, it holds once the journal closes
Function SetSetting(String id, Float value) Global Native

; every actor a mod set read only can be dressed again, the button for a mod that's gone
Function ClearReadOnly() Global Native


; ---- For other mods. What each call does and what stays the same: InventoryReforged API.txt ----

; 1 here. A later version adds calls and raises the number, a call once given keeps its name, arguments and meaning
Int Function GetApiVersion() Global Native

; The one the panel shows now: the player in the inventory, the actor in their trade (Actor.OpenInventory),
; None when no menu with the panel is up
Actor Function GetPanelActor() Global Native

; Whether a trade with this actor (Actor.OpenInventory) would show the panel now, with the player's settings
Bool Function ShowsPanelFor(Actor akActor) Global Native

; The panel shows this actor in their trade but puts nothing on and takes nothing off. Kept in the save until set
; back to False
Function SetReadOnly(Actor akActor, Bool abReadOnly) Global Native

; The script on this form gets both events below, kept in the save, registering twice is once:
;   Function OnInventoryReforgedWear(Actor akWearer, Form akItem, Bool abOn)
;     the player put a piece on someone through the panel or took one off, after the game did it
;   Function OnInventoryReforgedDone(Actor akWearer, Bool abChanged)
;     the trade closed, after its last wear event
Function RegisterForWear(Form akReceiver) Global Native
Function UnregisterForWear(Form akReceiver) Global Native
