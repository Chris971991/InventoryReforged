#pragma once

// Stolen copies of an item get a row of their own in the item menus, the way the game already lists a tempered or
// enchanted copy apart. The player sees what is stolen, and storing, taking, selling or dropping a row moves only the
// copies of that row
namespace StolenRows
{
	// how many calls in the item menus get patched, the trampoline needs room for each: 17 of the lists, 4 picks of
	// an extra list for a move and the 4 moves after them
	inline constexpr std::size_t kCalls = 25;

	// patches the calls, once when the plugin loads
	void Install();
}
