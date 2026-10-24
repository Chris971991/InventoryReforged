#include "PCH.h"

#include "Stacks.h"

#include "SafeRead.h"

#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#	define NOMINMAX
#endif
#include <Windows.h>
#include <TlHelp32.h>

#if IR_DEBUG_EDITION
#	include "Debug/Actions.h"
#	include "REL/Offset2ID.h"
#endif

namespace Stacks
{
	namespace
	{
#if IR_DEBUG_EDITION
		// the main thread's whole stack, SkyrimSE.exe gives it 1 MB. Whatever started the work it stands in sits at
		// the far end
		constexpr std::size_t kWords = 0x100000 / sizeof(std::uintptr_t);
		constexpr std::size_t kMainMost = 4000;
		constexpr std::size_t kRootPlaces = 30;
#else
		// 32 KB of a stack from its top, deep enough for the calls of a menu
		constexpr std::size_t kWords = 4096;
#endif
		// other threads only get their top read
		constexpr std::size_t kOtherWords = 4096;
		constexpr std::size_t kPlaces = 16;
		// how many places of the main thread go on its line, nearest first
		constexpr std::size_t kMainPlaces = 40;
#if !IR_DEBUG_EDITION
		constexpr std::size_t kMainMost = kMainPlaces;
#endif
		constexpr std::uint32_t kMostWrites = IR_DEBUG_EDITION ? 10 : 3;
		// one freeze is told once, the stuck work and the menu with no frame are the same freeze
		constexpr auto kOneFreeze = std::chrono::seconds(30);

		std::atomic<DWORD> mainThread{ 0 };
		std::atomic<std::uint32_t> written{ 0 };
		std::atomic<std::int64_t> lastWritten{ 0 };
		// only the watch thread fills it, one thread after another
		std::array<std::uintptr_t, kWords> words{};

		struct Module
		{
			std::uintptr_t start;
			std::uintptr_t end;
			std::string name;
			bool windows;
		};

		constexpr std::size_t kRegisters = 10;
		constexpr std::array kRegisterNames{ "rax"sv, "rbx"sv, "rcx"sv, "rdx"sv, "rsi"sv, "rdi"sv, "r12"sv, "r13"sv, "r14"sv, "r15"sv };

		struct Read
		{
			std::uintptr_t place = 0;
			std::size_t count = 0;
			bool done = false;
			std::array<std::uintptr_t, kRegisters> registers{};
			// the first two 32 bit words each register points at. A lock of the game keeps there the thread that holds
			// it for writing and its count, so a thread waiting on a lock tells who holds it
			std::array<std::array<std::uint32_t, 2>, kRegisters> pointed{};
			std::array<bool, kRegisters> readable{};
		};

		void ReadPointed(Read& a_read)
		{
			for (std::size_t at = 0; at < kRegisters; ++at) {
				a_read.readable[at] = SafeRead::Copy(a_read.pointed[at].data(), a_read.registers[at], sizeof(a_read.pointed[at]));
			}
		}

		// The stack from its top up to its base, at most a_most words. The committed pages rsp lies in end right at
		// the base, so the copy never runs past the stack. The thread is held still meanwhile, nothing here takes the
		// heap or a lock it may hold
		std::size_t CopyStack(std::uintptr_t a_top, std::size_t a_most)
		{
			MEMORY_BASIC_INFORMATION memory{};
			if (!VirtualQuery(reinterpret_cast<const void*>(a_top), &memory, sizeof(memory)) || memory.State != MEM_COMMIT) {
				return 0;
			}
			const auto base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
			const auto count = std::min({ a_most, kWords, (base - a_top) / sizeof(std::uintptr_t) });
			return SafeRead::Copy(words.data(), a_top, count * sizeof(std::uintptr_t)) ? count : 0;
		}

		Read ReadThread(DWORD a_id, std::size_t a_most)
		{
			Read read;
			const HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, a_id);
			if (!thread) {
				return read;
			}
			if (SuspendThread(thread) != static_cast<DWORD>(-1)) {
				alignas(16) CONTEXT context{};
				context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
				if (GetThreadContext(thread, &context)) {
					read.place = context.Rip;
					read.count = CopyStack(context.Rsp, a_most);
					read.registers = { context.Rax, context.Rbx, context.Rcx, context.Rdx, context.Rsi, context.Rdi, context.R12, context.R13, context.R14, context.R15 };
					ReadPointed(read);
					read.done = true;
				}
				ResumeThread(thread);
			}
			CloseHandle(thread);
			return read;
		}

		std::vector<Module> Modules()
		{
			std::vector<Module> modules;
			wchar_t folder[MAX_PATH]{};
			const auto length = GetSystemWindowsDirectoryW(folder, MAX_PATH);
			const std::wstring windows(folder, length);
			const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
			if (snapshot == INVALID_HANDLE_VALUE) {
				return modules;
			}
			MODULEENTRY32W entry{};
			entry.dwSize = sizeof(entry);
			for (auto more = Module32FirstW(snapshot, &entry); more; more = Module32NextW(snapshot, &entry)) {
				const std::wstring path(entry.szExePath);
				const bool fromWindows = !windows.empty() && _wcsnicmp(path.c_str(), windows.c_str(), windows.size()) == 0;
				const auto start = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
				modules.push_back({ start, start + entry.modBaseSize, SKSE::stl::utf16_to_utf8(entry.szModule).value_or("?"), fromWindows });
			}
			CloseHandle(snapshot);
			return modules;
		}

		const Module* ModuleOf(const std::vector<Module>& a_modules, std::uintptr_t a_place)
		{
			const auto found = std::ranges::find_if(a_modules, [&](const Module& a_module) { return a_place >= a_module.start && a_place < a_module.end; });
			return found == a_modules.end() ? nullptr : &*found;
		}

		bool Runnable(std::uintptr_t a_place)
		{
			// code that runs and can be read, a page of PAGE_EXECUTE alone faults on a read
			constexpr DWORD kRuns = PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
			MEMORY_BASIC_INFORMATION memory{};
			return VirtualQuery(reinterpret_cast<const void*>(a_place), &memory, sizeof(memory)) && memory.State == MEM_COMMIT && (memory.Protect & kRuns) != 0 &&
			       (memory.Protect & PAGE_GUARD) == 0;
		}

		// A word on a stack is most likely a return address when it points into code right after a call: E8 and its
		// offset, or FF /2 in one of its lengths
		bool AfterCall(std::uintptr_t a_place)
		{
			constexpr std::size_t kLongestCall = 7;
			// the bytes before the place, code[kLongestCall - n] lies n before it
			std::array<std::uint8_t, kLongestCall> code{};
			if (a_place < kLongestCall || !Runnable(a_place - 1) || !Runnable(a_place - kLongestCall) || !SafeRead::Copy(code.data(), a_place - kLongestCall, code.size())) {
				return false;
			}
			if (code[kLongestCall - 5] == 0xE8) {
				return true;
			}
			return std::ranges::any_of(std::array<std::size_t, 5>{ 2, 3, 4, 6, 7 }, [&](std::size_t a_length) {
				return code[kLongestCall - a_length] == 0xFF && ((code[kLongestCall - a_length + 1] >> 3) & 7) == 2;
			});
		}

#if IR_DEBUG_EDITION
		// The Address Library's numbers of the game's code, sorted by place. A place told by the number before it is the
		// same in every version of the game, an offset alone isn't. Made by the watch thread before it reads a stack
		std::optional<REL::Offset2ID> ids;
		std::atomic<bool> idsReady{ false };

		std::optional<REL::Offset2ID::value_type> IdBefore(std::uintptr_t a_offset)
		{
			if (!idsReady.load()) {
				return std::nullopt;
			}
			const auto after = std::upper_bound(ids->begin(), ids->end(), a_offset,
				[](std::uintptr_t a_at, const REL::Offset2ID::value_type& a_entry) { return a_at < a_entry.offset; });
			if (after == ids->begin()) {
				return std::nullopt;
			}
			return *std::prev(after);
		}

		std::string IdOf(std::uintptr_t a_offset)
		{
			const auto entry = IdBefore(a_offset);
			return entry ? fmt::format(" (id {}+{:X})", entry->id, a_offset - entry->offset) : std::string{};
		}

		// the places the main thread stood at in the sample before
		constexpr std::size_t kSampled = 16;
		std::array<std::uintptr_t, 4> sampledTop{};
#endif

		std::string Name(const std::vector<Module>& a_modules, std::uintptr_t a_place)
		{
			const auto* module = ModuleOf(a_modules, a_place);
#if IR_DEBUG_EDITION
			if (module && module->start == REL::Module::get().base()) {
				return fmt::format("{}+{:X}{}", module->name, a_place - module->start, IdOf(a_place - module->start));
			}
#endif
			return module ? fmt::format("{}+{:X}", module->name, a_place - module->start) : fmt::format("{:X}", a_place);
		}

		// where the thread is and the return addresses found on its stack after it, nearest first
		std::vector<std::uintptr_t> PlacesOf(const std::vector<Module>& a_modules, const Read& a_read, std::size_t a_most)
		{
			std::vector<std::uintptr_t> places{ a_read.place };
			for (std::size_t at = 0; at < a_read.count && places.size() < a_most; ++at) {
				if (ModuleOf(a_modules, words[at]) && AfterCall(words[at])) {
					places.push_back(words[at]);
				}
			}
			return places;
		}

		std::string Joined(const std::vector<Module>& a_modules, const std::vector<std::uintptr_t>& a_places, std::size_t a_from, std::size_t a_to)
		{
			std::string line;
			for (auto at = a_from; at < a_to && at < a_places.size(); ++at) {
				line += fmt::format("{}{}", line.empty() ? "" : " < ", Name(a_modules, a_places[at]));
			}
			return line;
		}

#if IR_DEBUG_EDITION
		// the stack is copied while the thread stands still, all of this runs once it goes on
		void TellActions(const std::vector<Module>& a_modules, const Read& a_read)
		{
			const auto* game = ModuleOf(a_modules, REL::Module::get().base());
			if (!game) {
				return;
			}
			std::vector<std::uintptr_t> seen(a_read.registers.begin(), a_read.registers.end());
			seen.insert(seen.end(), words.begin(), words.begin() + static_cast<std::ptrdiff_t>(a_read.count));
			logger::warn("  ActionScript the main thread is in, the nearest first: {}", Debug::Actions::OnStack(seen, game->start, game->end));
		}

		std::size_t Kilobytes(const Read& a_read)
		{
			return a_read.count * sizeof(std::uintptr_t) / 1024;
		}
#endif

		std::vector<DWORD> Threads()
		{
			std::vector<DWORD> threads;
			const auto process = GetCurrentProcessId();
			const auto self = GetCurrentThreadId();
			const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
			if (snapshot == INVALID_HANDLE_VALUE) {
				return threads;
			}
			THREADENTRY32 entry{};
			entry.dwSize = sizeof(entry);
			for (auto more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry)) {
				if (entry.th32OwnerProcessID == process && entry.th32ThreadID != self) {
					threads.push_back(entry.th32ThreadID);
				}
			}
			CloseHandle(snapshot);
			// the main thread first
			std::ranges::stable_partition(threads, [](DWORD a_id) { return a_id == mainThread.load(); });
			return threads;
		}
	}

	void MainThreadIsThis()
	{
		mainThread = GetCurrentThreadId();
	}

	bool OnMainThread()
	{
		return GetCurrentThreadId() == mainThread.load(std::memory_order_relaxed);
	}

	void Write(std::string_view a_why)
	{
		const auto now = std::chrono::steady_clock::now().time_since_epoch();
		const auto before = std::chrono::steady_clock::duration(lastWritten.load());
		if (before.count() != 0 && now - before < kOneFreeze) {
			return;
		}
		lastWritten = now.count();
		if (written.fetch_add(1) >= kMostWrites) {
			return;
		}
		const auto modules = Modules();
		const auto threads = Threads();
		logger::warn("where the threads of the game stand now ({}), game {}. Each place is module+offset, the first one is where the thread is, "
					 "the rest are return addresses found on its stack, the nearest first",
			a_why, REL::Module::get().version().string());
		std::size_t quiet = 0;
		// threads the main thread's registers point at, the one holding the lock it waits on is among them
		std::vector<std::uint32_t> named;
		for (const auto id : threads) {
			const bool main = id == mainThread.load();
			const auto read = ReadThread(id, main ? kWords : kOtherWords);
			if (!read.done) {
				continue;
			}
			// the thread runs again, from here on the heap and the log are free to use
			const auto places = PlacesOf(modules, read, main ? kMainMost : kPlaces);
			const bool ofMods = std::ranges::any_of(places, [&](std::uintptr_t a_place) {
				const auto* module = ModuleOf(modules, a_place);
				return module && !module->windows;
			});
			const bool pointedAt = std::ranges::find(named, id) != named.end();
			if (!main && !ofMods && !pointedAt) {
				++quiet;
				continue;
			}
			logger::warn("  thread {}{}{}: {}", id, main ? " (main)" : "", pointedAt ? " (named by a word the main thread points at)" : "",
				Joined(modules, places, 0, main ? kMainPlaces : places.size()));
			if (!main) {
				continue;
			}
			std::string registers;
			for (std::size_t at = 0; at < kRegisters; ++at) {
				if (read.readable[at]) {
					registers += fmt::format("{}{} {:X} -> {:08X} {:08X}", registers.empty() ? "" : ", ", kRegisterNames[at], read.registers[at], read.pointed[at][0], read.pointed[at][1]);
					named.push_back(read.pointed[at][0]);
				}
			}
			logger::warn("  the main thread's registers and the first two words where they point: {}", registers.empty() ? "none readable" : registers);
#if IR_DEBUG_EDITION
			// the far end of the stack shows what started the work, the 40 nearest places rarely reach it
			const auto from = std::max(kMainPlaces, places.size() > kRootPlaces ? places.size() - kRootPlaces : std::size_t{ 0 });
			logger::warn("  the main thread's stack is {} KB deep with {} places{}{}", Kilobytes(read), places.size(), from < places.size() ? "; the farthest ones, the root last: " : "",
				Joined(modules, places, from, places.size()));
			// words is filled again by the next thread, so this goes right now
			TellActions(modules, read);
#endif
		}
		logger::warn("  {} more threads stand in Windows' own code only", quiet);
	}

#if IR_DEBUG_EDITION
	void Prepare()
	{
		try {
			ids.emplace();
			idsReady = true;
		} catch (...) {
		}
	}

	void SampleMain()
	{
		const auto id = mainThread.load();
		if (id == 0) {
			return;
		}
		const auto modules = Modules();
		// the thread is held still only while its stack is copied, nothing is logged or made on the heap meanwhile
		const auto read = ReadThread(id, kWords);
		if (!read.done) {
			logger::warn("  the main thread could not be read");
			return;
		}
		const auto places = PlacesOf(modules, read, kSampled);
		std::array<std::uintptr_t, 4> top{};
		std::copy_n(places.begin(), std::min(places.size(), top.size()), top.begin());
		const bool same = top == sampledTop;
		sampledTop = top;
		logger::warn("  the main thread now{}, its stack {} KB deep: {}", same ? ", the same place as the sample before" : "", Kilobytes(read),
			Joined(modules, places, 0, places.size()));
		TellActions(modules, read);
	}

	// A wait stays at one place and a loop goes round a few functions. The thread is held only for the read of its
	// place, the counting happens after
	void SampleHot()
	{
		constexpr std::size_t kReads = 200;
		constexpr auto kApart = std::chrono::milliseconds(5);
		constexpr std::size_t kTold = 12;
		const auto id = mainThread.load();
		const HANDLE thread = id != 0 ? OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, id) : nullptr;
		if (!thread) {
			return;
		}
		std::array<std::uintptr_t, kReads> places{};
		std::size_t taken = 0;
		while (taken < kReads && SuspendThread(thread) != static_cast<DWORD>(-1)) {
			alignas(16) CONTEXT context{};
			context.ContextFlags = CONTEXT_CONTROL;
			const bool read = GetThreadContext(thread, &context);
			ResumeThread(thread);
			if (!read) {
				break;
			}
			places[taken++] = context.Rip;
			std::this_thread::sleep_for(kApart);
		}
		CloseHandle(thread);
		if (taken == 0) {
			return;
		}
		const auto modules = Modules();
		std::map<std::string, std::size_t> counts;
		for (std::size_t at = 0; at < taken; ++at) {
			const auto* module = ModuleOf(modules, places[at]);
			std::string where = module ? module->name : "outside any module";
			if (module && module->start == REL::Module::get().base()) {
				if (const auto entry = IdBefore(places[at] - module->start)) {
					where += fmt::format(" id {}", entry->id);
				}
			}
			++counts[where];
		}
		std::vector<std::pair<std::size_t, std::string>> sorted;
		for (const auto& [where, count] : counts) {
			sorted.emplace_back(count, where);
		}
		std::ranges::sort(sorted, std::ranges::greater{});
		std::string line;
		for (std::size_t at = 0; at < sorted.size() && at < kTold; ++at) {
			line += fmt::format("{}{} {}%", line.empty() ? "" : ", ", sorted[at].second, sorted[at].first * 100 / taken);
		}
		logger::warn("  where the main thread was in {} reads {} ms apart: {}", taken, kApart.count(), line);
	}
#endif
}
