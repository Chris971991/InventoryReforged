#pragma once

// Mods that keep the world running under the menus (Skyrim Souls RE) turn the camera while the pointer rests on
// an edge of the screen or a stick is pushed. The panel covers the screen, so that only turns the world behind it.
// While the inventory with the panel is open the look input of the player is dropped
namespace StillCamera
{
	void Listen();
}
