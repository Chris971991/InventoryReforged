#pragma once

// What the quests want with an item, told to its row of an item menu as equipmentQuest, equipmentQuestName and
// equipmentQuestPlace (the name is a place when the quest has no name of its own):
//   1 a running quest holds it as a quest object, the game won't let it go
//   2 a running quest holds it in an alias without that, so the quest follows it and losing it may break it
//   3 an objective in the journal names it, a quest that counts such things by script (bring ten of ...)
//   4 a quest holds it as a quest object but the quest is done, the item stays stuck for no reason
//   5 a running quest points at such an item: an alias forced on a copy of it placed in the world, an alias that
//     makes one, or a property of a script of the quest or of its aliases
//   6 the same for a quest that hasn't begun yet, or runs at stage 0 still waiting for the player
//   7 the same for a quest that is done or stopped after it ran
// 1, 2, 3, 5 and 6 say the item is still needed, 4 and 7 that it isn't. A quest that holds the item itself in an alias
// is how the game knows its items for sure, the words of an objective and what a quest points at are a guess, so
// those count only when nothing holds the item
namespace Quests
{
	// The objectives in the journal are old once an item menu opens or closes, the next row told reads them again,
	// the game stands still in a menu. What the quests point at is read a few quests each frame of the game, some
	// seconds after a load and again after every item menu. Rows before the first reading get marks from aliases
	// and the journal only
	void Refresh();
	void Forget();
	// a step of the reading after a load, on the main thread in the player's own update
	void Frame();
	void Tell(RE::GFxMovieView& a_view, RE::GFxValue& a_row, RE::InventoryEntryData& a_item);
}
