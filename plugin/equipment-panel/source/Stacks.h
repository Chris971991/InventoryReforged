#pragma once

// Where the threads of the game stand right now, written into the log when the game looks frozen. A freeze leaves
// no crash log, these lines name the code each thread waits or spins in
namespace Stacks
{
	// told once from the thread the game runs its frames on
	void MainThreadIsThis();
	bool OnMainThread();

	// a few times a game at most, a freeze that goes on is told once
	void Write(std::string_view a_why);

#if IR_DEBUG_EDITION
	// on the watch thread before it reads a stack: the numbers of the game's code that name its places
	void Prepare();
	// the main thread alone, one line, said to be at the same place as the sample before when it is
	void SampleMain();
	// where the main thread spends a stall, its place read many times and counted by function
	void SampleHot();
#endif
}
