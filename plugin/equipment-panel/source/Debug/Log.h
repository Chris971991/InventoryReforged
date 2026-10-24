#pragma once

#include "Profile.h"

// The log of the debug edition. Earlier sessions are kept beside it, every line goes to the disk at once and carries
// the number of the frame it was written in
namespace Debug
{
	using Clock = std::chrono::steady_clock;

	// in place of the log SKSE::Init makes, right after it
	void StartLog(const SKSE::LoadInterface& a_skse);

	// the files, plugins and settings the game runs with, once its data is loaded
	void TellEnvironment();

	// A frame goes by: the world's (the player's own update) or one drawn by a menu of the panel. Under a menu that lets
	// the world run both come in one frame, it's counted once
	void WorldFrame();
	void MenuFrame();
	std::uint64_t Frame();

	// whether a line of this kind may go out now, past five a second it's counted instead
	bool Allowed(std::string_view a_kind) noexcept;

	// A call into the game: one line before it, one after with the time it took. A call that comes more than five
	// times a second is counted instead
	class Bracket
	{
	public:
		template <class... Args>
		explicit Bracket(fmt::format_string<Args...> a_format, Args&&... a_args) noexcept
		{
			try {
				const auto kind = a_format.get();
				if (!Allowed({ kind.data(), kind.size() })) {
					return;
				}
				what = fmt::format(a_format, std::forward<Args>(a_args)...);
				logger::debug("-> {}", what);
			} catch (...) {
			}
		}

		~Bracket();
		Bracket(const Bracket&) = delete;
		Bracket& operator=(const Bracket&) = delete;

	private:
		std::string what;
		Clock::time_point start{ Clock::now() };
	};

	// Things that come too often for a line each: counted, and the counts written with each report. Never waits, a
	// count that would wait is dropped
	void Count(std::string_view a_what);
	// a line, or a count once the same kind of line came five times within a second
	void Said(std::string_view a_kind, std::string_view a_text);
	void TellCounts();

	template <class... Args>
	void Note(const Profile::Timer& a_timer, fmt::format_string<Args...> a_format, Args&&... a_args) noexcept
	{
		std::array<char, 96> text{};
		try {
			const auto end = fmt::format_to_n(text.data(), text.size() - 1, a_format, std::forward<Args>(a_args)...).out;
			a_timer.Note({ text.data(), static_cast<std::size_t>(end - text.data()) });
		} catch (...) {
		}
	}

	// for the lines of a stall: the memory of the machine, whether the game has the focus, the menus open now
	std::string Machine();
	void MenuChanged(const RE::BSFixedString& a_menu, bool a_opening);
	std::string OpenMenus();
}
