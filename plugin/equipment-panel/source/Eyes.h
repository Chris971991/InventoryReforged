#pragma once

// The eyes of the slots, drawn by a small movie of their own right after an item menu has drawn everything, its 3D
// too. The models of the slots come from the game's 3D pass over the whole menu, so an eye of the menu itself went
// behind a ring that fills its slot. The panel still owns the eyes, their clicks and their light: it only tells
// where each one is in sight and how it looks
namespace Eyes
{
	// every eye in sight, six numbers each: left, top (shares of the screen), size (share of its height), shut,
	// lit, colour. False when the movie of the eyes could not be loaded, the panel draws them itself then
	bool Show(const RE::GFxValue& a_numbers);
	// a copy of the open menu of a slot or a row, so it lies over the slot models and the window's model: numbers,
	// words and icon names as the panel hands them, nothing takes it away. False when the movie isn't there
	bool ShowMenu(const RE::GFxValue& a_told);
	// after an item menu drew itself
	void Display();
	// the menu closed
	void Clear();
	// hooks the drawing of the container and barter menus, the inventory has its own hook in main.cpp
	void Install();
}
