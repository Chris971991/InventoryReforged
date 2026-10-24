#pragma once

// Every row an item menu lists passes here on its way to the menu script and gets what the panel shows on it:
// when the player got it and whether that is new, a junk mark, how many the player carries of it (for a row of
// a chest or a shop), how many of its copies are stolen, the plugin it comes from and what the quests want with it
namespace Rows
{
	// hooks the rows of the item menus, called once when the plugin loads
	void Install();
	// listens to what moves in and out of the player's inventory, called once the game data is loaded
	void Listen();
	// an item menu opens
	void MenuOpened();
	// the file of the plugin that adds a form, null for the game and its DLC
	const char* Origin(const RE::TESForm& a_form);
#if IR_DEBUG_EDITION
	// a menu of the panel drew a frame: the rows told since the last one in one line
	void TellFill();
#endif
}
