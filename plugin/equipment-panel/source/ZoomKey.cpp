#include "PCH.h"

#include "ZoomKey.h"

namespace ZoomKey
{
	namespace
	{
		// the zoom call in the message handler of one menu, the script of that menu is asked
		template <class Menu>
		struct Key
		{
			static bool Thunk(RE::Inventory3DManager* a_preview)
			{
				IR_HOOK("the item zoom key");
				if (on && on()) {
					const auto menu = RE::UI::GetSingleton()->GetMenu(Menu::MENU_NAME);
					RE::GFxValue taken;
					IR_SCRIPT(ask);
					if (menu && menu->uiMovie && menu->uiMovie->Invoke(ask, &taken, nullptr, 0) && taken.IsBool() && taken.GetBool()) {
						IR_LOG("the panel took the item zoom key");
						return false;
					}
				}
				IR_LOG("the game zooms the item");
				const bool toggled = original(a_preview);
				// the game's own zoom is turned with the mouse over the item, a script may keep that area off otherwise
				if (const auto menu = RE::UI::GetSingleton()->GetMenu(Menu::MENU_NAME); told && menu && menu->uiMovie) {
					RE::GFxValue zooming(a_preview->GetRuntimeData().zoomDistance > 0.0F);
					IR_SCRIPT(told);
					menu->uiMovie->Invoke(told, nullptr, &zooming, 1);
				}
				return toggled;
			}

			static inline const char* ask = nullptr;
			static inline const char* told = nullptr;
			static inline bool (*on)() = nullptr;
			static inline REL::Relocation<decltype(Thunk)> original;
		};

		// A plugin loaded earlier may have put its own message handler into the table of the inventory, Apparel
		// Preview does. Such a handler hands the message on and the game's one gets it in the end, so the call is
		// patched in the game's handler all the same. Its place is read from the table in the game file
		std::optional<std::uintptr_t> GameEntry(const std::uintptr_t* a_entry)
		{
			const auto& module = REL::Module::get();
			const auto base = module.base();
			const auto* dos = reinterpret_cast<const REX::W32::IMAGE_DOS_HEADER*>(base);
			const auto* headers = reinterpret_cast<const REX::W32::IMAGE_NT_HEADERS64*>(base + dos->lfanew);
			const auto* sections = reinterpret_cast<const REX::W32::IMAGE_SECTION_HEADER*>(
				reinterpret_cast<std::uintptr_t>(&headers->optionalHeader) + headers->fileHeader.optionalHeaderSize);
			const auto place = reinterpret_cast<std::uintptr_t>(a_entry) - base;
			std::ifstream file(std::filesystem::path(module.filePath().data()), std::ios::binary);
			const auto read = [&](std::uint64_t a_at) -> std::optional<std::uint64_t> {
				std::uint64_t value = 0;
				file.seekg(static_cast<std::streamoff>(a_at));
				file.read(reinterpret_cast<char*>(&value), sizeof(value));
				return file ? std::optional{ value } : std::nullopt;
			};
			// the file keeps addresses for the base the game was built for, the game runs from another one
			const auto built = read(dos->lfanew + offsetof(REX::W32::IMAGE_NT_HEADERS64, optionalHeader) + offsetof(REX::W32::IMAGE_OPTIONAL_HEADER64, imageBase));
			for (std::uint16_t i = 0; built && i < headers->fileHeader.sectionCount; ++i) {
				const auto& section = sections[i];
				if (place >= section.virtualAddress && place + sizeof(std::uint64_t) <= section.virtualAddress + section.rawDataSize) {
					const auto entry = read(section.rawDataPtr + place - section.virtualAddress);
					return entry ? std::optional{ base + (*entry - *built) } : std::nullopt;
				}
			}
			return std::nullopt;
		}

		// The call sits in the message handler of the menu, found by what it calls so the place needn't be known for
		// every game version. A plugin loaded earlier that hooked that very call leaves nothing to find, the key then
		// keeps the game's own zoom. The search stays in the game's code
		template <class Menu>
		void Patch(const char* a_ask, const char* a_told, bool (*a_on)(), std::string_view a_menu)
		{
			Key<Menu>::ask = a_ask;
			Key<Menu>::told = a_told;
			Key<Menu>::on = a_on;
			constexpr std::size_t kProcessMessage = 4;
			constexpr std::size_t kLookAhead = 0x1000;
			constexpr std::size_t kCallSize = 5;
			constexpr std::uint8_t kCall = 0xE8;
			const REL::Relocation<std::uintptr_t> toggle{ RELOCATION_ID(50887, 51760) };
			const REL::Relocation<std::uintptr_t> table{ Menu::VTABLE[0] };
			const auto* entry = reinterpret_cast<const std::uintptr_t*>(table.address()) + kProcessMessage;
			auto handler = *entry;
			const auto code = REL::Module::get().segment(REL::Segment::textx);
			const auto end = code.address() + code.size();
			const auto game = REL::Module::get().version().string();
			const auto inGame = [&](std::uintptr_t a_place) { return a_place >= code.address() && a_place < end; };
			if (!inGame(handler)) {
				const auto own = GameEntry(entry);
				if (!own || !inGame(*own)) {
					logger::warn("the item zoom key stays the game's own, another plugin took the message handler of the {} first and the "
								 "game's own one couldn't be read from the game file (game {})",
						a_menu, game);
					return;
				}
				logger::info("another plugin took the message handler of the {} first, the game's own one is at {:X} in the game file", a_menu, *own - REL::Module::get().base());
				handler = *own;
			}
			for (std::size_t at = 0; at < kLookAhead && handler + at + kCallSize <= end; ++at) {
				const auto place = handler + at;
				if (*reinterpret_cast<const std::uint8_t*>(place) != kCall) {
					continue;
				}
				const auto target = place + kCallSize + *reinterpret_cast<const std::int32_t*>(place + 1);
				if (target == toggle.address()) {
					Key<Menu>::original = SKSE::GetTrampoline().write_call<5>(place, Key<Menu>::Thunk);
					logger::info("the item zoom key of the {} asks {} first, call at +{:X}", a_menu, a_ask, at);
					return;
				}
			}
			// another plugin may have hooked this very call, or this game version calls the zoom from elsewhere
			logger::warn("the item zoom key stays the game's own, its call wasn't found in the message handler of the {} (game {})", a_menu, game);
		}
	}

	void Install(const char* a_ask, const char* a_told, bool (*a_on)())
	{
		Patch<RE::InventoryMenu>(a_ask, a_told, a_on, "inventory");
	}

	void InstallTrade(const char* a_ask, const char* a_told, bool (*a_on)())
	{
		Patch<RE::ContainerMenu>(a_ask, a_told, a_on, "container menu");
	}
}
