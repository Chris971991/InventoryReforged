#pragma once

// The order in which the player got the things of the inventory, so a list can show the newest first
// Every thing that comes into the player's inventory gets the next number, the numbers live in the save
// A row of an item menu carries the number of its item as equipmentAdded, 0 for a thing held since before the mod.
// What came after the inventory was last closed is new, the row says so as equipmentNew
namespace Acquired
{
	// starts to listen to the game, called once the game data is loaded
	void Listen();

	std::uint32_t NumberOf(RE::FormID a_form);
	bool IsNew(std::uint32_t a_number);
	// the inventory closed, what it showed is seen now
	void Seen();

	void Save(SKSE::SerializationInterface* a_save);
	void Read(SKSE::SerializationInterface* a_save, std::uint32_t a_version);
	void Forget();
}
