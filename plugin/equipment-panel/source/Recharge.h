#pragma once

// Recharging enchanted weapons and staves from the menu of a row, on top of the game's own recharge of the player's
// weapon in the inventory. The weapons are of the one whose inventory is open: the player's in the inventory, the
// teammate's in their trade (the container menu in NPC mode). The gem is the player's or, in a trade, the teammate's.
// Either the row of a weapon asks for a gem or the row of a gem asks for a weapon, both through a list in the item
// card the way the game asks for a gem.
// The steps are the game's own of the inventory (ShowSoulGemList and ItemCardListCallback, 51858 and 51859): the soul
// a gem gives with the player's perks, the enchanting the player learns, a gem that stays as an empty one, the charge
// kept by the hand while a weapon is worn. Game 1.6 and later, the numbers are of those.
// A row is named by its place in the game's list of the menu (itemIndex of the menu script) and its item's form id,
// the menu is the one whose movie calls
namespace Recharge
{
	// How full the weapon of a row is, 0 to 100, or -1 for a row of someone else's or one that takes no charge
	float ChargeOf(const RE::GFxMovie* a_movie, std::int32_t a_index, RE::FormID a_form);

	// The item card's list of the soul gems for a weapon's row, as the game hands it to the inventory's card: type,
	// name, listItems of {text, owner, count, chargeAdded, itemIndex, formId}, currentCharge. Owner is empty for the
	// player's. Left undefined when the row can't be charged, and with no gem the game says so as it does
	void SoulGems(RE::GFxMovie& a_movie, std::int32_t a_index, RE::FormID a_form, RE::GFxValue& a_info);

	// The same kind of list of the drained weapons the gem of a row can charge, a line {text, count, charge,
	// chargeAdded, itemIndex, formId} each. Left undefined when there's none or the gem has no soul
	void Weapons(RE::GFxMovie& a_movie, std::int32_t a_index, RE::FormID a_form, RE::GFxValue& a_info);

	// Charges the weapon of one row with the gem of another. False when nothing was done
	bool Charge(const RE::GFxMovie* a_movie, std::int32_t a_weapon, RE::FormID a_weaponForm, std::int32_t a_gem, RE::FormID a_gemForm);

	// once a frame of a container menu: the rows of both sides are filled anew after a charge, once nothing holds a row
	void Frame();
}
