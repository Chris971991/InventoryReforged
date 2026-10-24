; What the text controls of the menu call, appended to the menu script interface.skyui_menu writes

; every hidden slot and piece is drawn on the player again, the plugin keeps them in the save
Function ShowEverything()
	InventoryReforged.ShowAllHidden()
EndFunction

; every follower a mod asked the panel to leave as they are can be dressed again
Function DressFollowersAgain()
	InventoryReforged.ClearReadOnly()
EndFunction
