#include "PCH.h"

#include "Profile.h"

#include "Stacks.h"

#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#	define NOMINMAX
#endif
#include <Windows.h>
#include <Psapi.h>

#if IR_DEBUG_EDITION
#	include <TlHelp32.h>

#	include "Debug/Script.h"
#endif

namespace Profile
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		// the debug edition looks closer and tells more
		constexpr bool kDebug = IR_DEBUG_EDITION;
		constexpr std::size_t kMostWorks = kDebug ? 128 : 64;
		// one frame at 60 fps lost to the plugin alone
		constexpr auto kSlow = std::chrono::milliseconds(kDebug ? 5 : 16);
#if IR_DEBUG_EDITION
		// a hook with the game's work inside runs long often enough, a menu's whole advance for one, only a real
		// hitch gets a line
		constexpr auto kSlowGame = std::chrono::milliseconds(50);
#endif
		constexpr std::uint32_t kMostSlowTold = kDebug ? std::numeric_limits<std::uint32_t>::max() : 30;
		constexpr double kMegabyte = 1024.0 * 1024.0;

		struct Counted
		{
			std::string_view name;
			std::atomic<std::uint64_t> calls{ 0 };
			std::atomic<std::uint64_t> nanoseconds{ 0 };
			std::atomic<std::uint64_t> longest{ 0 };
#if IR_DEBUG_EDITION
			// a work slow every frame is told once a second
			std::atomic<std::int64_t> slowToldAt{ 0 };
#endif
		};

		std::array<Counted, kMostWorks> works;
		std::size_t named = 0;
		std::mutex naming;
		std::atomic<std::uint32_t> slowTold{ 0 };

		std::array<std::atomic<std::int64_t>, static_cast<std::size_t>(Held::kCount)> held{};
		constexpr std::array kHeldNames{ "body copies"sv, "bone lists"sv, "scene models"sv, "held elsewhere when let go"sv };

		std::mutex reporting;
		std::string menu;
		Clock::time_point menuSince;
		std::atomic<std::uint64_t> menuFrames{ 0 };
		// frames come on the main thread only, the report reads the longest from another one
		std::optional<Clock::time_point> lastFrame;
		std::atomic<std::uint64_t> longestFrame{ 0 };
		std::uint64_t memoryBefore = 0;

		// What runs right now: a thread inside a kind of work since a moment. A Timer takes a place and gives it back,
		// the watch reads them from its own thread. A place with no moment yet is being taken or given back
		struct Running
		{
			std::atomic<std::uint32_t> thread{ 0 };
			std::atomic<std::size_t> work{ 0 };
			std::atomic<std::int64_t> since{ 0 };
			std::atomic<bool> told{ false };
			// a hook of the plugin with the game's own work inside it, its time isn't the plugin's
			std::atomic<bool> game{ false };
#if IR_DEBUG_EDITION
			// written by the thread that runs the work, read by the watch, a torn read only garbles the words
			std::array<char, 96> note{};
#endif
		};
		constexpr std::size_t kMostRunning = 64;
		constexpr std::size_t kNoPlace = kMostRunning;
		std::array<Running, kMostRunning> running;

		constexpr auto kStuck = std::chrono::seconds(kDebug ? 1 : 5);
		// a menu that stands still for a second is what a player calls a freeze, the stacks then say where the game was
		constexpr auto kStill = std::chrono::seconds(1);
		constexpr auto kLook = std::chrono::milliseconds(kDebug ? 50 : 250);
#if IR_DEBUG_EDITION
		// a hitch is told with what runs, no thread is held still for it. A stall goes on to a few samples of the
		// main thread, a place that stays the same is a wait and one that moves is a loop
		constexpr auto kHitch = std::chrono::milliseconds(300);
		// samples go on for minutes, a freeze on the stand once stood 7 minutes and the log went quiet after 8 s
		constexpr std::array kSamples{ std::chrono::seconds(2), std::chrono::seconds(4), std::chrono::seconds(8), std::chrono::seconds(16), std::chrono::seconds(32),
			std::chrono::seconds(64), std::chrono::seconds(128), std::chrono::seconds(256) };
		// the sample that also counts where the main thread spends its time
		constexpr std::size_t kHotSample = 1;
		// after the last sample a short line a minute, so a log of a game killed mid freeze still says how long it stood
		constexpr auto kStillEvery = std::chrono::seconds(60);
		constexpr auto kSlowToldEvery = std::chrono::seconds(1);
		constexpr auto kStuckThreadsRead = std::chrono::seconds(5);
		constexpr std::size_t kMostGrown = 8;
		std::atomic<bool> hitchTold{ false };
		std::atomic<std::size_t> samplesTaken{ 0 };
		std::atomic<std::int64_t> stillLastTold{ 0 };
#endif
		// whether the inventory is open now and when it drew its last frame, for the watch
		std::atomic<bool> menuShown{ false };
		// the watch reads it with no lock, a report may hold the lock of the menu's name
		std::atomic<const char*> shownMenu{ "item menu" };
		std::atomic<std::int64_t> lastFrameAt{ 0 };
		std::atomic<bool> stillTold{ false };
		// The part of the panel script that runs now and since when. From outside a menu script is just the game's
		// ActionScript, so a stall named none of the plugin's own work and left the panel's script and SkyUI's
		// alike. The panel tells the plugin when its build and its pass over the rows begin and end
		std::atomic<std::int64_t> scriptSince{ 0 };
		std::atomic<const char*> scriptPart{ "" };
		constexpr std::array kScriptParts{ std::pair{ "build"sv, "its build as the menu opens" }, std::pair{ "rows"sv, "its pass over the rows of the list" } };

		// all the plugin's own time, the outermost timer of a thread counts it so nested ones don't count twice
		std::atomic<std::uint64_t> ownNanoseconds{ 0 };
		thread_local std::uint32_t timersInside = 0;

		// stalls are watched this long after the menu's first frame
		constexpr auto kComingUp = std::chrono::seconds(6);

		struct Opening
		{
			std::string menu;
			Clock::time_point made;
			std::optional<Clock::time_point> first;
			Clock::time_point last;
			Clock::duration longest{};
			Clock::duration longestAt{};
			std::uint64_t ownBefore = 0;
			std::optional<Clock::time_point> scriptFrom;
			Clock::duration script{};
		};
		std::mutex openingLock;
		std::optional<Opening> opening;

		double Milliseconds(Clock::duration a_span)
		{
			return std::chrono::duration<double, std::milli>(a_span).count();
		}

		void TellOpening(const Opening& a_opening, Clock::time_point a_now)
		{
			const double own = static_cast<double>(ownNanoseconds.load() - a_opening.ownBefore) / 1e6;
			if (!a_opening.first) {
				logger::info("the {} closed {:.0f} ms after it was made without a frame, the plugin's own work {:.0f} ms, the panel script {:.0f} ms", a_opening.menu,
					Milliseconds(a_now - a_opening.made), own, Milliseconds(a_opening.script));
				return;
			}
			logger::info("the {} showed its first frame {:.0f} ms after it was made, its longest stall in the next {:.1f} s {:.0f} ms (at {:.0f} ms), the plugin's own work in "
						 "all that {:.0f} ms, the panel script {:.0f} ms",
				a_opening.menu, Milliseconds(*a_opening.first - a_opening.made), std::chrono::duration<double>(a_opening.last - *a_opening.first).count(),
				Milliseconds(a_opening.longest), Milliseconds(a_opening.longestAt), own, Milliseconds(a_opening.script));
		}

		std::int64_t Ticks(Clock::time_point a_at)
		{
			return a_at.time_since_epoch().count();
		}

		double SecondsSince(std::int64_t a_ticks, Clock::time_point a_now)
		{
			return std::chrono::duration<double>(a_now - Clock::time_point(Clock::duration(a_ticks))).count();
		}

		std::size_t Take(std::size_t a_work, Clock::time_point a_start, bool a_game)
		{
			const auto thread = static_cast<std::uint32_t>(GetCurrentThreadId());
			for (std::size_t at = 0; at < kMostRunning; ++at) {
				std::uint32_t free = 0;
				if (running[at].thread.compare_exchange_strong(free, thread)) {
					running[at].work.store(a_work);
					running[at].game.store(a_game);
					running[at].told.store(false);
#if IR_DEBUG_EDITION
					running[at].note[0] = '\0';
#endif
					running[at].since.store(Ticks(a_start));
					return at;
				}
			}
			return kNoPlace;
		}

		// the name of a running work, in the debug edition with the words it noted
		std::string NameOf(const Running& a_one)
		{
			std::string name{ works[a_one.work.load()].name };
			if (a_one.game.load()) {
				name += " (a hook of the plugin, the game's own work runs inside it)";
			}
#if IR_DEBUG_EDITION
			std::array<char, std::tuple_size_v<decltype(Running::note)>> note{};
			std::copy_n(a_one.note.begin(), note.size() - 1, note.begin());
			if (note[0] != '\0') {
				name += fmt::format(" ({})", note.data());
			}
#endif
			return name;
		}

#if IR_DEBUG_EDITION
		// the calls of each work when a stall began, only the watch touches it
		std::array<std::uint64_t, kMostWorks> callsAtStall{};

		void MarkStall()
		{
			for (std::size_t at = 0; at < kMostWorks; ++at) {
				callsAtStall[at] = works[at].calls.load(std::memory_order_relaxed);
			}
			Debug::Script::MarkStall();
		}

		// a work with calls had its name set before its first call, so the name is safe to read
		std::string CalledSinceStall()
		{
			std::vector<std::pair<std::uint64_t, std::string_view>> grown;
			for (std::size_t at = 0; at < kMostWorks; ++at) {
				if (const auto now = works[at].calls.load(std::memory_order_relaxed); now > callsAtStall[at]) {
					grown.emplace_back(now - callsAtStall[at], works[at].name);
				}
			}
			std::ranges::sort(grown, std::ranges::greater{});
			std::string told;
			for (std::size_t at = 0; at < grown.size() && at < kMostGrown; ++at) {
				told += fmt::format("{}{} {} times", told.empty() ? "" : ", ", grown[at].second, grown[at].first);
			}
			return told.empty() ? "none" : told;
		}
#endif

		// the work a thread is inside of now, the outermost first
		std::string WorkOf(std::uint32_t a_thread)
		{
			std::vector<std::pair<std::int64_t, std::string>> found;
			for (const auto& one : running) {
				const auto since = one.since.load();
				if (since != 0 && one.thread.load() == a_thread) {
					found.emplace_back(since, NameOf(one));
				}
			}
			std::ranges::sort(found);
			std::string told;
			for (const auto& [since, name] : found) {
				told += fmt::format("{}{}", told.empty() ? "" : " > ", name);
			}
			return told;
		}

		// every work running now with its thread
		std::string Busy()
		{
			std::string busy;
			for (const auto& one : running) {
				if (one.since.load() != 0) {
					busy += fmt::format("{}{} on thread {}", busy.empty() ? "" : ", ", NameOf(one), one.thread.load());
				}
			}
			return busy.empty() ? "none" : busy;
		}

		void Look()
		{
			const auto now = Clock::now();
			std::string stuck;
			for (auto& one : running) {
				const auto since = one.since.load();
				const auto thread = one.thread.load();
				if (since == 0 || thread == 0 || now - Clock::time_point(Clock::duration(since)) < kStuck || one.told.exchange(true)) {
					continue;
				}
				logger::warn("stuck: {} has run for {:.0f} s on thread {} ({})", NameOf(one), SecondsSince(since, now), thread, WorkOf(thread));
				stuck = works[one.work.load()].name;
			}
			const auto frame = lastFrameAt.load();
#if IR_DEBUG_EDITION
			// stuck work is told after a second, the threads are read only once it's stuck as long as the release waits:
			// a body built in a loading screen may take a second, and reading every thread holds each still a moment
			stuck.clear();
			for (const auto& one : running) {
				const auto since = one.since.load();
				if (since != 0 && one.thread.load() != 0 && now - Clock::time_point(Clock::duration(since)) >= kStuckThreadsRead) {
					stuck = works[one.work.load()].name;
				}
			}
			if (menuShown.load() && frame != 0) {
				const auto still = now - Clock::time_point(Clock::duration(frame));
				if (still >= kHitch && still < kStill && !hitchTold.exchange(true)) {
					logger::info("the {} has drawn no frame for {:.2f} s; the plugin's own work running now: {}; panel functions open: {}; the last ones that ended: {}", shownMenu.load(),
						SecondsSince(frame, now), Busy(), Debug::Script::Running(now), Debug::Script::Latest(5));
				}
				if (const auto taken = samplesTaken.load(); stillTold.load() && taken < kSamples.size() && still >= kSamples[taken]) {
					samplesTaken = taken + 1;
					logger::warn("the {} still draws no frame after {:.1f} s; panel functions open: {}; the plugin's own work: {}; since the stall began the plugin's works ran {}, "
								 "panel functions began {}",
						shownMenu.load(), SecondsSince(frame, now), Debug::Script::Running(now), Busy(), CalledSinceStall(), Debug::Script::BegunSinceStall());
					Stacks::SampleMain();
					if (taken == kHotSample) {
						Stacks::SampleHot();
					}
				}
				if (stillTold.load() && samplesTaken.load() == kSamples.size() && still >= kSamples.back()) {
					const auto last = stillLastTold.load();
					if (last == 0 || now - Clock::time_point(Clock::duration(last)) >= kStillEvery) {
						stillLastTold = Ticks(now);
						logger::warn("the {} still draws no frame after {:.0f} s", shownMenu.load(), SecondsSince(frame, now));
					}
				}
			}
#endif
			if (menuShown.load() && frame != 0 && now - Clock::time_point(Clock::duration(frame)) >= kStill && !stillTold.exchange(true)) {
#if IR_DEBUG_EDITION
				MarkStall();
#endif
				const auto busy = Busy();
				const auto script = scriptSince.load();
				const auto panelScript = script != 0 ? fmt::format("running {} for {:.1f} s", scriptPart.load(), SecondsSince(script, now)) : "not running"s;
				// what isn't watched can't be ruled out, so the line says what it saw and stops there
				logger::warn("the {} has drawn no frame for {:.1f} s; the plugin's own work running now: {}; the panel's script: {}", shownMenu.load(), SecondsSince(frame, now),
					busy, panelScript);
#if IR_DEBUG_EDITION
				logger::warn("  panel functions open: {}; the last ones that ended: {}; {}; menus open: {}", Debug::Script::Running(now), Debug::Script::Latest(10),
					Debug::Machine(), Debug::OpenMenus());
#endif
				Stacks::Write("an item menu draws no frame");
			} else if (!stuck.empty()) {
				Stacks::Write(fmt::format("stuck in {}", stuck));
			}
		}

		std::uint64_t PrivateMemory()
		{
			PROCESS_MEMORY_COUNTERS_EX counters{};
			counters.cb = sizeof(counters);
			if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
				return 0;
			}
			return counters.PrivateUsage;
		}

		double Milliseconds(std::uint64_t a_nanoseconds)
		{
			return static_cast<double>(a_nanoseconds) / 1e6;
		}

#if IR_DEBUG_EDITION
		// the panel writes its state into the log once the menu draws again after a stall
		void ReportAfterStall()
		{
			const std::string_view name = shownMenu.load();
			SKSE::GetTaskInterface()->AddUITask([name] {
				const auto menu = RE::UI::GetSingleton()->GetMenu(name);
				if (menu && menu->uiMovie) {
					IR_SCRIPT("_global.Equipment.panelReport");
					const RE::GFxValue note("after a stall");
					menu->uiMovie->Invoke("_global.Equipment.panelReport", nullptr, &note, 1);
				}
			});
		}

		std::size_t Threads()
		{
			std::size_t count = 0;
			const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
			if (snapshot == INVALID_HANDLE_VALUE) {
				return 0;
			}
			THREADENTRY32 entry{};
			entry.dwSize = sizeof(entry);
			for (auto more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry)) {
				count += entry.th32OwnerProcessID == GetCurrentProcessId() ? 1 : 0;
			}
			CloseHandle(snapshot);
			return count;
		}
#endif
	}

	std::size_t Work(std::string_view a_name)
	{
		const std::scoped_lock lock{ naming };
		for (std::size_t at = 0; at < named; ++at) {
			if (works[at].name == a_name) {
				return at;
			}
		}
		// past the last place everything shares it, there are far fewer kinds of work than that
		if (named == kMostWorks) {
			return kMostWorks - 1;
		}
		works[named].name = a_name;
		return named++;
	}

	Timer::Timer(std::size_t a_work, bool a_game) :
		work(a_work),
		game(a_game),
		start(Clock::now())
	{
		slot = Take(work, start, game);
		if (!game) {
			++timersInside;
		}
	}

	Timer::~Timer()
	{
		const auto took = Clock::now() - start;
		const auto nanoseconds = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(took).count());
		if (!game && --timersInside == 0) {
			ownNanoseconds.fetch_add(nanoseconds, std::memory_order_relaxed);
		}
		auto& counted = works[work];
		if (slot != kNoPlace) {
			if (running[slot].told.load()) {
				logger::info("{} ended after {:.1f} s", counted.name, std::chrono::duration<double>(took).count());
			}
			running[slot].since.store(0);
			running[slot].thread.store(0);
		}
		counted.calls.fetch_add(1, std::memory_order_relaxed);
		counted.nanoseconds.fetch_add(nanoseconds, std::memory_order_relaxed);
		auto longest = counted.longest.load(std::memory_order_relaxed);
		while (nanoseconds > longest && !counted.longest.compare_exchange_weak(longest, nanoseconds, std::memory_order_relaxed)) {}
#if IR_DEBUG_EDITION
		if (took >= (game ? kSlowGame : kSlow)) {
			const auto now = Ticks(Clock::now());
			auto toldAt = counted.slowToldAt.load();
			if (Clock::duration(now - toldAt) >= kSlowToldEvery && counted.slowToldAt.compare_exchange_strong(toldAt, now)) {
				const auto thread = static_cast<std::uint32_t>(GetCurrentThreadId());
				const auto under = WorkOf(thread);
				logger::info("slow: {} took {:.1f} ms on thread {}{}", counted.name, Milliseconds(nanoseconds), thread, under.empty() ? "" : ", under " + under);
			}
		}
#else
		if (took >= kSlow && slowTold.fetch_add(1) < kMostSlowTold) {
			logger::info("slow: {} took {:.1f} ms", counted.name, Milliseconds(nanoseconds));
		}
#endif
	}

#if IR_DEBUG_EDITION
	void Timer::Note(std::string_view a_text) const
	{
		if (slot == kNoPlace) {
			return;
		}
		auto& note = running[slot].note;
		const auto length = std::min(a_text.size(), note.size() - 1);
		std::copy_n(a_text.begin(), length, note.begin());
		note[length] = '\0';
	}
#endif

	void Made(Held a_held, std::int64_t a_count)
	{
		held[static_cast<std::size_t>(a_held)].fetch_add(a_count, std::memory_order_relaxed);
	}

	void Freed(Held a_held, std::int64_t a_count)
	{
		held[static_cast<std::size_t>(a_held)].fetch_sub(a_count, std::memory_order_relaxed);
	}

	void MenuOpened(std::string_view a_menu)
	{
		const std::scoped_lock lock{ reporting };
		menu = a_menu;
		menuSince = Clock::now();
		menuFrames = 0;
		longestFrame = 0;
		lastFrameAt = Ticks(menuSince);
		stillTold = false;
#if IR_DEBUG_EDITION
		hitchTold = false;
		samplesTaken = 0;
		stillLastTold = 0;
		// the magic menu has the panel too
		constexpr std::array kWatched{ RE::InventoryMenu::MENU_NAME, RE::ContainerMenu::MENU_NAME, RE::BarterMenu::MENU_NAME, RE::MagicMenu::MENU_NAME };
#else
		constexpr std::array kWatched{ RE::InventoryMenu::MENU_NAME, RE::ContainerMenu::MENU_NAME, RE::BarterMenu::MENU_NAME };
#endif
		// frames are counted in the drawing of the item menus
		menuShown = false;
		for (const auto name : kWatched) {
			if (a_menu == name) {
				shownMenu = name.data();
				menuShown = true;
			}
		}
	}

	void MenuMade(std::string_view a_menu)
	{
		MenuOpened(a_menu);
		const std::scoped_lock lock{ openingLock };
		opening = Opening{ .menu = std::string(a_menu), .made = Clock::now(), .ownBefore = ownNanoseconds.load() };
	}

	void ScriptStarts(std::string_view a_part)
	{
		const auto part = std::ranges::find(kScriptParts, a_part, &decltype(kScriptParts)::value_type::first);
		scriptPart = part != kScriptParts.end() ? part->second : "a part with no name";
		scriptSince = Ticks(Clock::now());
		const std::scoped_lock lock{ openingLock };
		if (opening) {
			opening->scriptFrom = Clock::now();
		}
	}

	void ScriptStops()
	{
		scriptSince = 0;
		const std::scoped_lock lock{ openingLock };
		if (opening && opening->scriptFrom) {
			opening->script += Clock::now() - *opening->scriptFrom;
			opening->scriptFrom.reset();
		}
	}

	void MenuFrame()
	{
		const auto now = Clock::now();
		{
			const std::scoped_lock lock{ openingLock };
			if (opening && !opening->first) {
				opening->first = now;
				opening->last = now;
			} else if (opening) {
				const auto took = now - opening->last;
				opening->last = now;
				if (took > opening->longest) {
					opening->longest = took;
					opening->longestAt = now - opening->made;
				}
				if (now - *opening->first >= kComingUp) {
					TellOpening(*opening, now);
					opening.reset();
				}
			}
		}
		// the first frame after an open waits for the menu to load, it isn't a frame of the menu
		if (lastFrame && menuFrames.load(std::memory_order_relaxed) > 0) {
			const auto took = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - *lastFrame).count());
			if (took > longestFrame.load(std::memory_order_relaxed)) {
				longestFrame.store(took, std::memory_order_relaxed);
			}
		}
		lastFrame = now;
		menuFrames.fetch_add(1, std::memory_order_relaxed);
		const auto before = lastFrameAt.exchange(Ticks(now));
#if IR_DEBUG_EDITION
		Debug::MenuFrame();
		samplesTaken = 0;
		stillLastTold = 0;
		const bool hitch = hitchTold.exchange(false);
		if (stillTold.exchange(false)) {
			logger::info("the {} draws again after {:.1f} s", shownMenu.load(), SecondsSince(before, now));
			ReportAfterStall();
		} else if (hitch) {
			logger::info("the {} draws again after {:.2f} s", shownMenu.load(), SecondsSince(before, now));
		}
#else
		if (stillTold.exchange(false)) {
			logger::info("the inventory draws again after {:.1f} s", SecondsSince(before, now));
		}
#endif
	}

	void Report(std::string_view a_when)
	{
		{
			const std::scoped_lock lock{ openingLock };
			if (opening) {
				TellOpening(*opening, Clock::now());
				opening.reset();
			}
		}
		const std::scoped_lock lock{ reporting };
		std::string head = fmt::format("profile, {}", a_when);
		if (!menu.empty()) {
			const double seconds = std::chrono::duration<double>(Clock::now() - menuSince).count();
			head += fmt::format(", {} was open {:.1f} s", menu, seconds);
			if (const auto frames = menuFrames.exchange(0); frames > 0 && seconds > 0.0) {
				head += fmt::format(", {} frames, {:.0f} fps, longest frame {:.1f} ms", frames, static_cast<double>(frames) / seconds, Milliseconds(longestFrame.exchange(0)));
			}
			menu.clear();
			menuShown = false;
		}
		logger::info("{}", head);

		struct Row
		{
			std::string_view name;
			std::uint64_t calls;
			std::uint64_t nanoseconds;
			std::uint64_t longest;
		};
		std::vector<Row> rows;
		{
			const std::scoped_lock names{ naming };
			for (std::size_t at = 0; at < named; ++at) {
				auto& counted = works[at];
				if (const auto calls = counted.calls.exchange(0); calls > 0) {
					rows.push_back({ counted.name, calls, counted.nanoseconds.exchange(0), counted.longest.exchange(0) });
				}
			}
		}
		std::ranges::sort(rows, std::ranges::greater{}, &Row::nanoseconds);
		std::string own;
		for (const auto& row : rows) {
			own += fmt::format("{}{} {} x {:.3f} ms (longest {:.2f})", own.empty() ? "" : ", ", row.name, row.calls, Milliseconds(row.nanoseconds) / static_cast<double>(row.calls), Milliseconds(row.longest));
		}
		logger::info("profile own time: {}", own.empty() ? "nothing ran" : own);

		std::string kept;
		for (std::size_t at = 0; at < held.size(); ++at) {
			kept += fmt::format("{}{} {}", at == 0 ? "" : ", ", kHeldNames[at], held[at].load());
		}
		const auto memory = PrivateMemory();
		const double change = (static_cast<double>(memory) - static_cast<double>(memoryBefore)) / kMegabyte;
		logger::info("profile held: {}, game memory {:.0f} MB ({:+.0f} since the last report)", kept, static_cast<double>(memory) / kMegabyte, memoryBefore == 0 ? 0.0 : change);
		memoryBefore = memory;
#if IR_DEBUG_EDITION
		DWORD handles = 0;
		GetProcessHandleCount(GetCurrentProcess(), &handles);
		logger::info("profile machine: {}, {} threads and {} handles in the game", Debug::Machine(), Threads(), handles);
		Debug::Script::Report();
		Debug::TellCounts();
#endif
	}

	// The watch reads only the plugin's own counters and writes the log, it never touches the game
	void Watch()
	{
		static std::once_flag started;
		std::call_once(started, [] {
			std::thread([] {
#if IR_DEBUG_EDITION
				Stacks::Prepare();
				Debug::TellEnvironment();
#endif
				for (;;) {
					std::this_thread::sleep_for(kLook);
					Look();
				}
			}).detach();
		});
	}
}
