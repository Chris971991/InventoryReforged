#include "PCH.h"

#include "Debug/Script.h"

#include "Debug/Log.h"
#include "Rows.h"
#include "Stacks.h"

namespace Debug::Script
{
	namespace
	{
		using Params = RE::GFxFunctionHandler::Params;

		// every function of the debug panel has a number, the build gives them out and checks they fit
		constexpr std::size_t kMostEntries = 4096;
		constexpr std::size_t kNameSize = 64;
		constexpr std::size_t kDeepest = 64;
		constexpr std::size_t kMostGrown = 8;
		constexpr std::size_t kKept = 256;
		constexpr std::size_t kLongestText = 4000;
		constexpr auto kSlow = std::chrono::milliseconds(50);
		constexpr auto kSlowToldEvery = std::chrono::seconds(1);
		// the debug panel says it started in its first frame, a menu that waits longer has none
		constexpr auto kWordWithin = std::chrono::seconds(3);

		std::array<std::array<char, kNameSize>, kMostEntries> names{};
		std::atomic<std::uint32_t> namedCount{ 0 };

		// how many times each function began, on any thread. The watch keeps the counts from when a stall began, a
		// loop that keeps calling the panel shows up even if no sample catches it inside
		std::array<std::atomic<std::uint64_t>, kMostEntries> entered{};
		std::array<std::uint64_t, kMostEntries> enteredAtStall{};

		struct Open
		{
			std::atomic<std::uint32_t> id{ 0 };
			std::atomic<const char*> kind{ "" };
			std::atomic<std::int64_t> since{ 0 };
		};
		std::array<Open, kDeepest> open;
		std::atomic<std::size_t> depth{ 0 };
		// entries past the deepest place are only counted, their ends match these first
		std::size_t tooDeep = 0;

		struct Ended
		{
			std::atomic<std::uint32_t> id{ 0 };
			std::atomic<std::int64_t> at{ 0 };
			std::atomic<std::int64_t> took{ 0 };
		};
		std::array<Ended, kKept> ended;
		std::atomic<std::uint64_t> endedCount{ 0 };

		struct Counted
		{
			std::atomic<std::uint64_t> calls{ 0 };
			std::atomic<std::uint64_t> nanoseconds{ 0 };
			std::atomic<std::uint64_t> longest{ 0 };
			std::int64_t slowToldAt = 0;
		};
		std::array<Counted, kMostEntries> counted;

		// the movies the panel went into, main thread only
		struct Movie
		{
			const RE::GFxMovie* view = nullptr;
			const char* kind = "";
			Clock::time_point made;
			bool started = false;
			bool told = false;
		};
		std::array<Movie, 8> movies;
		std::size_t nextMovie = 0;

		std::int64_t Ticks(Clock::time_point a_at)
		{
			return a_at.time_since_epoch().count();
		}

		Clock::time_point At(std::int64_t a_ticks)
		{
			return Clock::time_point(Clock::duration(a_ticks));
		}

		double Milliseconds(Clock::duration a_span)
		{
			return std::chrono::duration<double, std::milli>(a_span).count();
		}

		std::string Name(std::uint32_t a_id)
		{
			if (a_id < kMostEntries && names[a_id][0] != '\0') {
				return std::string(names[a_id].data());
			}
			return fmt::format("entry {}", a_id);
		}

		Movie* MovieOf(const RE::GFxMovie* a_view)
		{
			for (auto& movie : movies) {
				if (movie.view && movie.view == a_view) {
					return &movie;
				}
			}
			return nullptr;
		}

		std::uint32_t Id(const Params& a_params)
		{
			return a_params.argCount > 0 && a_params.args[0].IsNumber() ? static_cast<std::uint32_t>(a_params.args[0].GetNumber()) : kMostEntries;
		}

		std::string_view Text(const Params& a_params, std::uint32_t a_at)
		{
			if (a_params.argCount <= a_at || !a_params.args[a_at].IsString() || !a_params.args[a_at].GetString()) {
				return {};
			}
			const std::string_view text = a_params.args[a_at].GetString();
			return text.substr(0, kLongestText);
		}

		// the names of the open entries from place a_from up to a_to
		std::string Chain(std::size_t a_from, std::size_t a_to)
		{
			std::string told;
			for (std::size_t at = a_from; at < a_to && at < kDeepest; ++at) {
				told += (told.empty() ? "" : " > ") + Name(open[at].id.load());
			}
			return told;
		}

		// the open functions are the main thread's, the thread that makes a menu runs a bit of the panel too and
		// would mix into them
		void Enter(Params& a_params)
		{
			const auto id = Id(a_params);
			if (id < kMostEntries) {
				entered[id].fetch_add(1, std::memory_order_relaxed);
			}
			if (!Stacks::OnMainThread()) {
				return;
			}
			const auto at = depth.load();
			if (id >= kMostEntries || at >= kDeepest) {
				tooDeep += id < kMostEntries ? 1 : 0;
				return;
			}
			open[at].id = id;
			open[at].kind = KindOf(a_params.movie);
			open[at].since = Ticks(Clock::now());
			depth = at + 1;
		}

		void Leave(Params& a_params)
		{
			const auto id = Id(a_params);
			if (id >= kMostEntries || !Stacks::OnMainThread()) {
				return;
			}
			if (tooDeep > 0) {
				--tooDeep;
				return;
			}
			const auto now = Clock::now();
			const auto top = depth.load();
			std::size_t at = top;
			while (at > 0 && open[at - 1].id.load() != id) {
				--at;
			}
			if (at == 0) {
				return;
			}
			--at;
			if (at + 1 < top) {
				logger::warn("panel functions {} never ended, {} ended over them: Scaleform may have stopped a script short, or one threw", Chain(at + 1, top), Name(id));
			}
			const auto took = now - At(open[at].since.load());
			const auto nanoseconds = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(took).count());
			depth = at;
			auto& record = ended[endedCount.fetch_add(1) % kKept];
			record.id = id;
			record.at = Ticks(now);
			record.took = took.count();
			auto& count = counted[id];
			count.calls.fetch_add(1, std::memory_order_relaxed);
			count.nanoseconds.fetch_add(nanoseconds, std::memory_order_relaxed);
			if (nanoseconds > count.longest.load(std::memory_order_relaxed)) {
				count.longest.store(nanoseconds, std::memory_order_relaxed);
			}
			if (took >= kSlow && now - At(count.slowToldAt) >= kSlowToldEvery) {
				count.slowToldAt = Ticks(now);
				logger::info("slow panel function: {} took {:.0f} ms in {}{}", Name(id), Milliseconds(took), open[at].kind.load(), at > 0 ? ", under " + Chain(0, at) : "");
			}
		}

		// names of the functions from number a_first on, each part of the panel sends its own
		void Names(Params& a_params)
		{
			if (a_params.argCount < 1 || !a_params.args[0].IsArray()) {
				return;
			}
			const auto first = a_params.argCount > 1 && a_params.args[1].IsNumber() ? static_cast<std::uint32_t>(a_params.args[1].GetNumber()) : 0u;
			const auto& list = a_params.args[0];
			for (std::uint32_t at = 0; at < list.GetArraySize() && first + at < kMostEntries; ++at) {
				auto& slot = names[first + at];
				RE::GFxValue name;
				if (slot[0] != '\0' || !list.GetElement(at, &name) || !name.IsString() || !name.GetString()) {
					continue;
				}
				const std::string_view text = name.GetString();
				std::ranges::copy(text.substr(0, kNameSize - 1), slot.begin());
				namedCount.fetch_add(1, std::memory_order_relaxed);
			}
		}

		void Log(Params& a_params)
		{
			logger::info("panel {}: {}", KindOf(a_params.movie), Text(a_params, 0));
		}

		void Said(Params& a_params)
		{
			Debug::Said(Text(a_params, 0), fmt::format("panel {}: {}", KindOf(a_params.movie), Text(a_params, 1)));
		}

		void Count(Params& a_params)
		{
			Debug::Count(Text(a_params, 0));
		}

		// the panel's word on which build it is, the plugin's own should be the same version and edition
		void Started(Params& a_params)
		{
			const auto stamp = Text(a_params, 0);
			auto* movie = MovieOf(a_params.movie);
			if (movie) {
				movie->started = true;
			}
			const auto version = SKSE::GetPluginVersion();
			const auto own = fmt::format("{}.{}.{} debug", version.major(), version.minor(), version.patch());
			logger::info("the panel script started in {}: {}", KindOf(a_params.movie), stamp);
			if (!stamp.starts_with(own)) {
				logger::warn("the panel script is {}, the plugin {}: files of two builds are mixed", stamp, own);
			}
		}

		template <void (*Run)(Params&)>
		class Handler : public RE::GFxFunctionHandler
		{
		public:
			void Call(Params& a_params) override
			{
				try {
					Run(a_params);
				} catch (...) {
				}
			}
		};

		template <void (*Run)(Params&)>
		void Add(RE::GFxMovieView* a_view, RE::GFxValue* a_root, const char* a_name)
		{
			// kept for the whole game, as the plugin's other functions
			static auto* const handler = new Handler<Run>();
			RE::GFxValue function;
			a_view->CreateFunction(&function, handler);
			a_root->SetMember(a_name, function);
		}
	}

	const char* KindOf(const RE::GFxMovie* a_movie)
	{
		const auto* movie = MovieOf(a_movie);
		return movie ? movie->kind : "a menu";
	}

	void MovieMade(const RE::GFxMovieView& a_view, const char* a_kind)
	{
		auto* movie = MovieOf(&a_view);
		if (!movie) {
			movie = &movies[nextMovie++ % movies.size()];
		}
		*movie = { &a_view, a_kind, Clock::now() };
	}

	void AddFunctions(RE::GFxMovieView* a_view, RE::GFxValue* a_root)
	{
		if (!a_view || !a_root) {
			return;
		}
		Add<Names>(a_view, a_root, "Names");
		Add<Enter>(a_view, a_root, "Enter");
		Add<Leave>(a_view, a_root, "Leave");
		Add<Log>(a_view, a_root, "Log");
		Add<Said>(a_view, a_root, "Said");
		Add<Count>(a_view, a_root, "Count");
		Add<Started>(a_view, a_root, "Started");
	}

	void FrameDrawn(const RE::GFxMovie* a_movie)
	{
		Rows::TellFill();
		if (const auto at = depth.load(); at > 0) {
			logger::warn("panel functions {} never ended by the frame: Scaleform may have stopped a script short, or one threw", Chain(0, at));
			depth = 0;
			tooDeep = 0;
		}
		auto* movie = MovieOf(a_movie);
		if (movie && !movie->started && !movie->told && Clock::now() - movie->made >= kWordWithin) {
			movie->told = true;
			logger::warn("no word from the panel script in {} {:.0f} s after it was loaded: panel.swf is missing, it's a release panel.swf beside "
						 "this debug plugin, or the panel never found the menu",
				movie->kind, std::chrono::duration<double>(Clock::now() - movie->made).count());
		}
	}

	std::string Running(Clock::time_point a_now)
	{
		const auto at = std::min(depth.load(), kDeepest);
		std::string told;
		for (std::size_t place = 0; place < at; ++place) {
			told += fmt::format("{}{} in {} for {:.2f} s", told.empty() ? "" : " > ", Name(open[place].id.load()), open[place].kind.load(),
				std::chrono::duration<double>(a_now - At(open[place].since.load())).count());
		}
		return told.empty() ? fmt::format("none of the {} functions of the panel", namedCount.load()) : told;
	}

	void MarkStall()
	{
		for (std::size_t id = 0; id < kMostEntries; ++id) {
			enteredAtStall[id] = entered[id].load(std::memory_order_relaxed);
		}
	}

	std::string BegunSinceStall()
	{
		std::vector<std::pair<std::uint64_t, std::uint32_t>> grown;
		for (std::uint32_t id = 0; id < kMostEntries; ++id) {
			if (const auto now = entered[id].load(std::memory_order_relaxed); now > enteredAtStall[id]) {
				grown.emplace_back(now - enteredAtStall[id], id);
			}
		}
		std::ranges::sort(grown, std::ranges::greater{});
		std::string told;
		for (std::size_t at = 0; at < grown.size() && at < kMostGrown; ++at) {
			told += fmt::format("{}{} {} times", told.empty() ? "" : ", ", Name(grown[at].second), grown[at].first);
		}
		return told.empty() ? "none" : told;
	}

	std::string Latest(std::size_t a_count)
	{
		const auto count = endedCount.load();
		const auto now = Clock::now();
		std::string told;
		for (std::size_t back = 1; back <= a_count && back <= count && back <= kKept; ++back) {
			const auto& record = ended[(count - back) % kKept];
			told += fmt::format("{}{} {:.1f} ms {:.1f} s ago", told.empty() ? "" : ", ", Name(record.id.load()), Milliseconds(Clock::duration(record.took.load())),
				std::chrono::duration<double>(now - At(record.at.load())).count());
		}
		return told.empty() ? "none" : told;
	}

	void Report()
	{
		struct Row
		{
			std::uint32_t id;
			std::uint64_t calls;
			std::uint64_t nanoseconds;
			std::uint64_t longest;
		};
		std::vector<Row> rows;
		for (std::uint32_t id = 0; id < kMostEntries; ++id) {
			if (const auto calls = counted[id].calls.exchange(0); calls > 0) {
				rows.push_back({ id, calls, counted[id].nanoseconds.exchange(0), counted[id].longest.exchange(0) });
			}
		}
		if (rows.empty()) {
			return;
		}
		// every function of the panel reports now, the ones that took the most time are plenty for a line
		constexpr std::size_t kRowsTold = 40;
		std::ranges::sort(rows, std::ranges::greater{}, &Row::nanoseconds);
		std::string told;
		for (std::size_t at = 0; at < rows.size() && at < kRowsTold; ++at) {
			const auto& row = rows[at];
			told += fmt::format("{}{} {} x {:.2f} ms (longest {:.1f})", told.empty() ? "" : ", ", Name(row.id), row.calls,
				static_cast<double>(row.nanoseconds) / 1e6 / static_cast<double>(row.calls), static_cast<double>(row.longest) / 1e6);
		}
		logger::info("panel functions since the last report, the costliest first: {}{}", told,
			rows.size() > kRowsTold ? fmt::format(", and {} more", rows.size() - kRowsTold) : std::string{});
	}
}
