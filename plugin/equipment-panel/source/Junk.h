#pragma once

// What the player marked as junk, by the item's base form, so every copy of it is junk. The marks live in the save
// and follow a plugin that moved in the load order. A sale or a store of all junk never takes what is worn,
// a favorite or anything a quest wants, the panel checks that on the rows
namespace Junk
{
	bool Has(RE::FormID a_form);
	// marks or unmarks the item, false when there is no such item
	bool Set(RE::FormID a_form, bool a_junk);

	void Save(SKSE::SerializationInterface* a_save);
	void Read(SKSE::SerializationInterface* a_save, std::uint32_t a_version);
	void Forget();
}
