#pragma once

// The panel is a script file of its own, Interface/InventoryReforged/panel.swf. It's loaded into the item menus
// of SkyUI as they open, so no file of SkyUI is replaced and any SkyUI version with the same classes works
namespace Inject
{
	// called for every menu file the game loads, takes the inventory, container and barter menus
	void IntoMenu(RE::GFxMovieView& a_view);

	// whether the movie is one of the menus the panel goes into, the rows of other menus get nothing of it
	bool HoldsPanel(const RE::GFxMovieView& a_view);
}
