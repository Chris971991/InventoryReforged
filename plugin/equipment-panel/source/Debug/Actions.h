#pragma once

// The ActionScript the main thread is inside of, read off its stack while the game stands still. Scaleform runs the
// code of every SWF in one interpreter, so the native places alone can't say whose script it is. Words of the stack
// lead to the blocks of code the interpreter runs, kept byte for byte as the SWF has them, and to the functions it
// called in them. Each gets named by the names in its own code
namespace Debug::Actions
{
	// a_words: the main thread's registers, then its stack from the top. Runs on the watch thread once the main thread
	// goes on, reads nothing but memory and calls nothing of the game
	std::string OnStack(std::span<const std::uintptr_t> a_words, std::uintptr_t a_gameStart, std::uintptr_t a_gameEnd);
}
