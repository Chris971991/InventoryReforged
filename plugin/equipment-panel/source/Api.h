#pragma once

// What other mods call: the global functions of the script InventoryReforged for Papyrus and the events it sends.
// What they do and what stays the same is in "InventoryReforged API.txt" of the release
namespace Api
{
	// raised by one for every call added, a call once given is never taken away or changed
	inline constexpr std::int32_t kVersion = 1;
	// the save records of the scripts that listen and of the actors shown but not dressed
	inline constexpr std::uint32_t kRecord = 'WEAR';
	inline constexpr std::uint32_t kReadOnlyRecord = 'RDON';

	void Bind(RE::BSScript::IVirtualMachine& a_machine);

	// OnInventoryReforgedWear(akWearer, akItem, abOn) to every script that listens, the game equipped what the player
	// chose in the panel
	void Worn(RE::Actor& a_wearer, RE::TESForm& a_item, bool a_on);
	// OnInventoryReforgedDone(akWearer, abChanged), the trade with the panel closed after its last wear event
	void Done(RE::Actor& a_wearer, bool a_changed);

	void Save(SKSE::SerializationInterface* a_save);
	void Load(SKSE::SerializationInterface* a_save, std::uint32_t a_record);
	void Revert();
}
