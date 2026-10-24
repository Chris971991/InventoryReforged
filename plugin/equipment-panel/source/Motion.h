#pragma once

// Mods that animate the player as a piece goes on or off, Immersive Equipping Animations and the like, send the
// player's animation graph an event. Under an item menu the game stands still and plays it only after the menu
// closes, so while such a move runs the panel steps the player's graph itself and the doll takes the bones over.
// With unpaused menus (Skyrim Souls RE) the game moves the graph and the doll only copies. After a paused menu the
// world gets the events again and plays them as it would have
namespace Motion
{
	// hooks the animation events of the player, once when the plugin loads
	void Install();
	// every frame of the inventory
	void Frame();
	// an item menu closed
	void MenuClosed();
}
