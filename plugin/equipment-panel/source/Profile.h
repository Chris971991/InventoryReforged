#pragma once

// The plugin's own time and what it holds, cheap enough to stay on for every player. What was counted goes into
// the log when an item menu closes and when the game saves, so a log sent with a bug report tells what the mod cost
namespace Profile
{
	// a kind of work timed under its name, got once and kept
	std::size_t Work(std::string_view a_name);

	// times the work from here to the end of the scope, a call longer than a frame is told in the log at once and one
	// that runs for seconds is told by the watch while it still runs
	class Timer
	{
	public:
		// a_game: a hook that calls the game's own function inside, its time stays out of the plugin's own
		explicit Timer(std::size_t a_work, bool a_game = false);
		~Timer();
		Timer(const Timer&) = delete;
		Timer& operator=(const Timer&) = delete;
#if IR_DEBUG_EDITION
		// a few words on what the work does right now, the lines of a stall and of stuck work print them
		void Note(std::string_view a_text) const;
#endif

	private:
		std::size_t work;
		bool game;
		std::size_t slot;
		std::chrono::steady_clock::time_point start;
	};

	// A thread of its own looks once a second at what the plugin is doing. Work stuck for seconds and an open
	// inventory that drew no frame for seconds go into the log right then, so a frozen game leaves the step it froze in
	// and whether the plugin's own code was running at all. Started once the game data is loaded
	void Watch();

	// what the plugin makes and lets go. Between menus these come back to zero, one that keeps growing is a leak
	enum class Held : std::size_t
	{
		kBodyCopies,
		kBoneLists,
		kSceneModels,
		// copies something else still held when the plugin let them go
		kHeldElsewhere,
		kCount
	};

	void Made(Held a_held, std::int64_t a_count = 1);
	void Freed(Held a_held, std::int64_t a_count = 1);

	void MenuOpened(std::string_view a_menu);
	void MenuFrame();

	// The panel goes into an item menu's movie while the game makes the menu. From here to a few seconds after its first
	// frame the opening is timed, one line in the log tells how long the menu took to show, its longest stall and how much
	// of that was the plugin's own work and the panel script
	void MenuMade(std::string_view a_menu);
	// a build of the panel script begins and ends, its time counts for the opening and a stall names it
	void ScriptStarts(std::string_view a_part);
	void ScriptStops();

	// writes what was counted since the last report and starts counting anew
	void Report(std::string_view a_when);
}
