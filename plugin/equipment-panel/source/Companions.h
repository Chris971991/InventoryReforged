#pragma once

// The companions a menu of items switches to: the player's inventory to a companion's trade, a trade to another
// companion's or back to the inventory. A companion is a teammate of the player near enough to be loaded, whom a
// trade would show on the panel
namespace Companions
{
	struct Companion
	{
		RE::FormID formId;
		std::string name;
	};

	// the companions near the player, nearest first, none while the tabs are switched off in the menu
	std::vector<Companion> Near();
	// Shuts a_from, the menu of items the player is in, and opens the trade with this companion after it, the player's
	// inventory for 0. False when the actor is no companion now or that menu isn't up
	bool SwitchTo(RE::FormID a_actor, const char* a_from);
	// a menu closed, a switch that waited for it opens the next one
	void MenuClosed(const RE::BSFixedString& a_menu);
}
