#pragma once

// What the panel script runs. The build of the debug panel puts a call at the start and at every way out of each of
// its functions, so each one tells the plugin when it begins and ends (scripts/interface/equipment/debug_calls.py).
// A freeze with none of them open is outside the panel's own code. The open ones are the main thread's, the watch
// reads them from its own
namespace Debug::Script
{
	using Clock = std::chrono::steady_clock;

	// the panel went into this menu's movie, its lines name the menu by it
	void MovieMade(const RE::GFxMovieView& a_view, const char* a_kind);
	const char* KindOf(const RE::GFxMovie* a_movie);

	// the functions only the debug panel calls: Names, Enter, Leave, Log, Said, Count, Started
	void AddFunctions(RE::GFxMovieView* a_view, RE::GFxValue* a_root);

	// A menu of the panel drew a frame. No entry runs between frames, one still open was cut short (Scaleform stops a
	// script that goes too deep), and a panel that never said it started is told once. The rows told since the frame
	// before go into one line
	void FrameDrawn(const RE::GFxMovie* a_movie);

	// for the lines of a stall: the functions open now with how long, and the last ones that ended
	std::string Running(Clock::time_point a_now);
	std::string Latest(std::size_t a_count);

	// the watch keeps the counts when a stall begins, then tells which functions began since and how often
	void MarkStall();
	std::string BegunSinceStall();

	// the entries' calls and times since the last report, one line
	void Report();
}
