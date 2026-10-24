#include "PCH.h"

#include "Debug/Log.h"

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>

#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#	define NOMINMAX
#endif
#include <Windows.h>
#include <TlHelp32.h>

namespace Debug
{
	namespace
	{
		// sessions kept beside the newest log, InventoryReforged-1.log is the one before
		constexpr int kKept = 5;

		std::atomic<std::uint64_t> frame{ 0 };
		// main thread only
		bool worldSinceMenu = false;

		class FrameFlag : public spdlog::custom_flag_formatter
		{
		public:
			void format(const spdlog::details::log_msg&, const std::tm&, spdlog::memory_buf_t& a_dest) override
			{
				const fmt::format_int number(frame.load(std::memory_order_relaxed));
				a_dest.append(number.data(), number.data() + number.size());
			}

			std::unique_ptr<custom_flag_formatter> clone() const override { return std::make_unique<FrameFlag>(); }
		};

		// The last line of a session the game closed itself. A log without it ended in a crash or the game was killed.
		// It's written as the plugin is let go, other threads may be gone by then with a lock of the log held, so it
		// goes past the log straight into the file
		struct SessionEnd
		{
			std::wstring path;

			~SessionEnd()
			{
				if (path.empty()) {
					return;
				}
				const HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL, nullptr);
				if (file == INVALID_HANDLE_VALUE) {
					return;
				}
				SYSTEMTIME now{};
				GetLocalTime(&now);
				std::array<char, 128> line{};
				const int length = std::snprintf(line.data(), line.size(), "[%02u:%02u:%02u.%03u] the session ended normally, the game closed\r\n", now.wHour, now.wMinute,
					now.wSecond, now.wMilliseconds);
				DWORD written = 0;
				if (length > 0) {
					WriteFile(file, line.data(), static_cast<DWORD>(length), &written, nullptr);
				}
				CloseHandle(file);
			}
		};
		SessionEnd sessionEnd;

		std::string Written(const FILETIME& a_time)
		{
			FILETIME local{};
			SYSTEMTIME at{};
			if (!FileTimeToLocalFileTime(&a_time, &local) || !FileTimeToSystemTime(&local, &at)) {
				return "?";
			}
			return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}", at.wYear, at.wMonth, at.wDay, at.wHour, at.wMinute);
		}

		// size and time of a file, or that it isn't there as a loose file
		std::string FileState(const std::filesystem::path& a_file)
		{
			WIN32_FILE_ATTRIBUTE_DATA data{};
			if (!GetFileAttributesExW(a_file.c_str(), GetFileExInfoStandard, &data)) {
				return "not a loose file";
			}
			const auto size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
			return fmt::format("{} bytes, written {}", size, Written(data.ftLastWriteTime));
		}

		std::string OwnFile()
		{
			HMODULE own = nullptr;
			std::array<wchar_t, MAX_PATH> path{};
			if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&StartLog), &own) ||
				!GetModuleFileNameW(own, path.data(), static_cast<DWORD>(path.size()))) {
				return "?";
			}
			return FileState(path.data());
		}

		std::string Now()
		{
			SYSTEMTIME now{};
			GetLocalTime(&now);
			return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
		}

		struct Counted
		{
			std::array<char, 48> name{};
			std::uint64_t count = 0;
			std::uint32_t lines = 0;
			std::int64_t second = 0;
		};
		constexpr std::size_t kMostCounted = 192;
		constexpr std::uint32_t kLinesASecond = 5;
		std::array<Counted, kMostCounted> counted;
		std::size_t countedUsed = 0;
		std::mutex countLock;

		// under countLock, null once the table is full
		Counted* Find(std::string_view a_name)
		{
			const auto name = a_name.substr(0, std::tuple_size_v<decltype(Counted::name)> - 1);
			for (std::size_t at = 0; at < countedUsed; ++at) {
				if (std::string_view(counted[at].name.data()) == name) {
					return &counted[at];
				}
			}
			if (countedUsed == kMostCounted) {
				return nullptr;
			}
			auto& made = counted[countedUsed++];
			std::ranges::copy(name, made.name.begin());
			return &made;
		}

		// the menus open now, kept by the thread of the interface and read by the watch
		constexpr std::size_t kMostMenus = 24;
		std::array<std::atomic<const char*>, kMostMenus> menus{};
	}

	void StartLog(const SKSE::LoadInterface& a_skse)
	{
		try {
			auto folder = SKSE::log::log_directory();
			if (!folder) {
				SKSE::log::init();
				return;
			}
			const std::string name{ SKSE::GetPluginName() };
			const auto file = [&](int a_number) { return *folder / (a_number == 0 ? name + ".log" : fmt::format("{}-{}.log", name, a_number)); };
			std::error_code failed;
			std::filesystem::create_directories(*folder, failed);
			// each one moves a place down over the one there, the oldest goes
			for (int number = kKept; number > 0; --number) {
				std::filesystem::rename(file(number - 1), file(number), failed);
			}
			auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file(0).string(), true);
			auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
			log->set_level(spdlog::level::trace);
			log->flush_on(spdlog::level::trace);
			auto formatter = std::make_unique<spdlog::pattern_formatter>();
			formatter->add_flag<FrameFlag>('*').set_pattern("[%T.%e] [%=5t] [f%*] [%L] %v");
			log->set_formatter(std::move(formatter));
			spdlog::set_default_logger(std::move(log));
			sessionEnd.path = file(0).wstring();

			const auto skse = a_skse.SKSEVersion();
			logger::info("{} v{}, the debug edition: every step goes into the log, the sessions before are {}-1.log to {}-{}.log", name, SKSE::GetPluginVersion().string("."),
				name, name, kKept);
			logger::info("session of {} local time, the plugin's file {}", Now(), OwnFile());
			// the stand's game hangs on qqq with no mod at all, a missing last line alone proves no crash
			logger::info("the last line \"the session ended normally\" comes when the game closes itself. Without it the game crashed, was killed, or hung on its way out, which "
						 "Skyrim does with no mods too");
			logger::info("game {}, SKSE {}.{}.{}, the game's main thread is {}", a_skse.RuntimeVersion().string("."), (skse >> 24) & 0xFF, (skse >> 16) & 0xFF, (skse >> 4) & 0xFFF,
				GetCurrentThreadId());
			logger::info("a line is [time] [thread] [f and the frame] [level]. Frames are the world's and those the menus of the panel draw");
		} catch (...) {
			SKSE::log::init();
		}
	}

	void TellEnvironment()
	{
		try {
			const auto data = std::filesystem::current_path() / "Data";
			for (const auto* file : { "Interface/InventoryReforged/panel.swf", "Interface/InventoryReforged/eyes.swf", "Interface/inventorymenu.swf",
					 "Interface/containermenu.swf", "Interface/bartermenu.swf", "Interface/magicmenu.swf", "SKSE/Plugins/InventoryReforged.ini",
					 "MCM/Settings/InventoryReforged.ini", "SKSE/Plugins/InventoryReforged/pages.json" }) {
				logger::info("file Data/{}: {}", file, FileState(data / file));
			}

			wchar_t windows[MAX_PATH]{};
			const auto length = GetSystemWindowsDirectoryW(windows, MAX_PATH);
			const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
			if (snapshot == INVALID_HANDLE_VALUE) {
				return;
			}
			std::vector<std::string> modules;
			MODULEENTRY32W entry{};
			entry.dwSize = sizeof(entry);
			for (auto more = Module32FirstW(snapshot, &entry); more; more = Module32NextW(snapshot, &entry)) {
				if (length > 0 && _wcsnicmp(entry.szExePath, windows, length) == 0) {
					continue;
				}
				const auto version = REL::GetFileVersion(entry.szExePath);
				modules.push_back(fmt::format("{} {}", SKSE::stl::utf16_to_utf8(entry.szModule).value_or("?"), version ? version->string(".") : "-"));
			}
			CloseHandle(snapshot);
			std::ranges::sort(modules);
			std::string told;
			for (const auto& module : modules) {
				told += told.empty() ? module : ", " + module;
			}
			logger::info("{} files of code outside Windows run in the game: {}", modules.size(), told);
		} catch (...) {
		}
	}

	void WorldFrame()
	{
		frame.fetch_add(1, std::memory_order_relaxed);
		worldSinceMenu = true;
	}

	void MenuFrame()
	{
		if (!std::exchange(worldSinceMenu, false)) {
			frame.fetch_add(1, std::memory_order_relaxed);
		}
	}

	std::uint64_t Frame()
	{
		return frame.load(std::memory_order_relaxed);
	}

	Bracket::~Bracket()
	{
		try {
			if (!what.empty()) {
				logger::debug("<- {}, {:.1f} ms", what, std::chrono::duration<double, std::milli>(Clock::now() - start).count());
			}
		} catch (...) {
		}
	}

	void Count(std::string_view a_what)
	{
		const std::unique_lock lock{ countLock, std::try_to_lock };
		if (!lock) {
			return;
		}
		if (auto* one = Find(a_what)) {
			++one->count;
		}
	}

	bool Allowed(std::string_view a_kind) noexcept
	{
		const std::unique_lock lock{ countLock, std::try_to_lock };
		auto* one = lock ? Find(a_kind) : nullptr;
		if (!one) {
			return true;
		}
		const auto second = std::chrono::duration_cast<std::chrono::seconds>(Clock::now().time_since_epoch()).count();
		if (one->second != second) {
			one->second = second;
			one->lines = 0;
		}
		if (one->lines < kLinesASecond) {
			++one->lines;
			return true;
		}
		++one->count;
		return false;
	}

	void Said(std::string_view a_kind, std::string_view a_text)
	{
		if (Allowed(a_kind)) {
			logger::info("{}", a_text);
		}
	}

	void TellCounts()
	{
		std::vector<std::pair<std::uint64_t, std::string>> rows;
		{
			const std::unique_lock lock{ countLock, std::try_to_lock };
			if (!lock) {
				return;
			}
			for (std::size_t at = 0; at < countedUsed; ++at) {
				if (const auto count = std::exchange(counted[at].count, 0); count > 0) {
					rows.emplace_back(count, counted[at].name.data());
				}
			}
		}
		if (rows.empty()) {
			return;
		}
		std::ranges::sort(rows, std::ranges::greater{});
		std::string told;
		for (const auto& [count, name] : rows) {
			told += fmt::format("{}{} {}", told.empty() ? "" : ", ", name, count);
		}
		logger::info("counted since the last report: {}", told);
	}

	std::string Machine()
	{
		constexpr double kMegabyte = 1024.0 * 1024.0;
		MEMORYSTATUSEX memory{};
		memory.dwLength = sizeof(memory);
		std::string told = GlobalMemoryStatusEx(&memory) ?
		                       fmt::format("memory load {}%, {:.0f} of {:.0f} MB free, page file {:.0f} MB free", memory.dwMemoryLoad, static_cast<double>(memory.ullAvailPhys) / kMegabyte,
								   static_cast<double>(memory.ullTotalPhys) / kMegabyte, static_cast<double>(memory.ullAvailPageFile) / kMegabyte) :
		                       std::string("memory unknown");
		DWORD process = 0;
		GetWindowThreadProcessId(GetForegroundWindow(), &process);
		told += process == GetCurrentProcessId() ? ", the game has the focus" : ", another program has the focus";
		return told;
	}

	void MenuChanged(const RE::BSFixedString& a_menu, bool a_opening)
	{
		const char* name = a_menu.c_str();
		if (!name) {
			return;
		}
		for (auto& slot : menus) {
			if (slot.load() == name) {
				if (!a_opening) {
					slot = nullptr;
				}
				return;
			}
		}
		if (a_opening) {
			for (auto& slot : menus) {
				const char* free = nullptr;
				if (slot.compare_exchange_strong(free, name)) {
					return;
				}
			}
		}
	}

	std::string OpenMenus()
	{
		std::string told;
		for (const auto& slot : menus) {
			if (const char* name = slot.load()) {
				told += fmt::format("{}{}", told.empty() ? "" : ", ", name);
			}
		}
		return told.empty() ? "none" : told;
	}
}
