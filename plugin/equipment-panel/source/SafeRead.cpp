#include "PCH.h"

#include "SafeRead.h"

#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#	define NOMINMAX
#endif
#include <Windows.h>

namespace SafeRead
{
	namespace
	{
		constexpr std::uintptr_t kLowest = 0x10000;
		constexpr std::uintptr_t kHighest = 0x7FFFFFFF0000;
		constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

		// VirtualQuery is a plain system call with no lock of the process, fine while another thread is held still
		Regions::Region Ask(std::uintptr_t a_at) noexcept
		{
			MEMORY_BASIC_INFORMATION memory{};
			if (!VirtualQuery(reinterpret_cast<const void*>(a_at), &memory, sizeof(memory))) {
				return { a_at, a_at + 1, false };
			}
			const auto start = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
			const bool readable = memory.State == MEM_COMMIT && (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0 && (memory.Protect & kReadable) != 0;
			return { start, start + memory.RegionSize, readable };
		}

		bool Readable(std::uintptr_t a_at, std::size_t a_size, Regions* a_regions) noexcept
		{
			if (a_at < kLowest || a_size == 0 || a_at > kHighest - a_size) {
				return false;
			}
			for (auto at = a_at; at < a_at + a_size;) {
				Regions::Region region{};
				bool known = false;
				if (a_regions) {
					for (const auto& one : a_regions->known) {
						if (at >= one.start && at < one.end) {
							region = one;
							known = true;
							break;
						}
					}
				}
				if (!known) {
					region = Ask(at);
					if (a_regions) {
						a_regions->known[a_regions->next++ % a_regions->known.size()] = region;
					}
				}
				if (!region.readable) {
					return false;
				}
				at = region.end;
			}
			return true;
		}

		bool CopyRaw(void* a_to, std::uintptr_t a_from, std::size_t a_size) noexcept
		{
			__try {
				std::memcpy(a_to, reinterpret_cast<const void*>(a_from), a_size);
				return true;
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				return false;
			}
		}
	}

	bool Copy(void* a_to, std::uintptr_t a_from, std::size_t a_size) noexcept
	{
		return Readable(a_from, a_size, nullptr) && CopyRaw(a_to, a_from, a_size);
	}

	bool Copy(void* a_to, std::uintptr_t a_from, std::size_t a_size, Regions& a_regions) noexcept
	{
		return Readable(a_from, a_size, &a_regions) && CopyRaw(a_to, a_from, a_size);
	}
}
