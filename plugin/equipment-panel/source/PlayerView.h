#pragma once

// Shows a copy of the player's model in the menu's own 3D scene, the one item previews are drawn in
// The copy is lit by the menu lights and nothing in the world can stand in front of it
namespace PlayerView
{
	// The room the player gets: its middle from the left and from the top of the screen,
	// its width and height as shares of the screen height
	struct Place
	{
		float centerX;
		float centerY;
		float width;
		float height;
	};

	// true when the copy was made anew, because the body, the room or the size and the place the player gave the
	// doll in the inventory changed since the last one
	bool Show(const Place& a_place);
	void Hide();
	// the menu closed, the next one starts with the doll facing front
	void Closed();

	// The tween menu or a menu with the panel opened with none of them up yet. A camera that looks the player in
	// the face right then was turned there by the player, mods that show the player in menus turn it after that
	void MenusOpening();
	// the last of those menus closed
	void MenusClosed();

	// turns the doll around its upright axis, a_degrees adds to the turn it has
	void Turn(float a_degrees);

	// the doll up now or null, and how many dolls were made so far, a new one stands in its own pose again
	RE::NiAVObject* Doll();
	std::size_t Made();

	// The game put the player's worn pieces on the body again. With the doll up that is a mod trying a piece on
	// or dressing the body its own way (Apparel Preview, outfit editors), the panel then follows the body again
	void BodyRebuilt();

	// a frame of an item menu with the doll, says what the worn pass marked
	void Frame();
}
