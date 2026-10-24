#pragma once

// Reading memory the plugin doesn't own, without a crash. Only committed readable pages that aren't guard pages get
// read. A guard page read under __try gets used up quietly, and its thread dies later when its stack grows
namespace SafeRead
{
	// what Windows said about a few regions, kept for one look only since memory changes between looks
	struct Regions
	{
		struct Region
		{
			std::uintptr_t start = 0;
			std::uintptr_t end = 0;
			bool readable = false;
		};

		std::array<Region, 32> known{};
		std::size_t next = 0;
	};

	bool Copy(void* a_to, std::uintptr_t a_from, std::size_t a_size) noexcept;
	bool Copy(void* a_to, std::uintptr_t a_from, std::size_t a_size, Regions& a_regions) noexcept;
}
