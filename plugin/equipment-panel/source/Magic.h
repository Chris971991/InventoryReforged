#pragma once

// The panel in the magic menu: what the player holds in each hand and what the voice has ready
namespace Magic
{
	enum Place : std::int32_t
	{
		kRight,
		kLeft,
		kVoice,
		kPlaces
	};

	struct Held
	{
		RE::TESForm* form = nullptr;
		// a weapon, shield, torch or scroll of the inventory, the magic list has no row for it
		bool item = false;
		// a two-handed weapon or a bow takes the other hand as well
		bool bothHands = false;
	};

	std::array<Held, kPlaces> Hands();
	// what anyone holds, a follower in a trade casts spells too
	std::array<Held, kPlaces> Hands(RE::Actor& a_actor);

	// takes the item of a hand off, spells stay as the game keeps them, it has no way to take one off
	bool TakeOff(Place a_place);
}
