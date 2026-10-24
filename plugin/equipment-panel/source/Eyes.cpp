#include "PCH.h"

#include "Eyes.h"

#include "Profile.h"
#include "Scene.h"

namespace Eyes
{
	namespace
	{
		constexpr auto kMovie = "InventoryReforged/eyes";
		constexpr std::uint32_t kNumbersOfAnEye = 6;

		// a movie is loaded into a menu, this one is never put on the stack of menus
		class Holder : public RE::IMenu
		{
		};

		Holder* holder = nullptr;
		RE::GPtr<RE::GFxMovieView> movie;
		bool failed = false;
		bool shown = false;
		bool menuShown = false;

		bool Load()
		{
			if (movie || failed) {
				return movie != nullptr;
			}
			holder = new Holder();
			// stretched over the whole screen, the movie draws by shares of it and keeps its circles round itself
			if (!RE::BSScaleformManager::GetSingleton()->LoadMovie(holder, movie, kMovie, RE::GFxMovieView::ScaleModeType::kExactFit, 0.0F) || !movie) {
				failed = true;
				logger::warn("Interface/{}.swf did not load, the panel draws the eyes itself, under the slot models", kMovie);
				return false;
			}
			// its first frame defines the drawing
			movie->Advance(0.0F);
			logger::info("the eyes of the slots are drawn over the 3D of the item menus");
			return true;
		}

		template <class Menu>
		struct PostDisplay
		{
			static void thunk(RE::IMenu* a_menu)
			{
				Profile::MenuFrame();
				IR_FRAME_DRAWN(a_menu);
				original(a_menu);
				Display();
			}

			static void Install()
			{
				constexpr std::size_t kPostDisplay = 6;
				REL::Relocation<std::uintptr_t> table{ Menu::VTABLE[0] };
				original = table.write_vfunc(kPostDisplay, thunk);
			}

			static inline REL::Relocation<decltype(thunk)> original;
		};
	}

	bool Show(const RE::GFxValue& a_numbers)
	{
		if (!a_numbers.IsArray() || !Load()) {
			return false;
		}
		// a value of one movie means nothing to another, the numbers are copied over
		RE::GFxValue numbers;
		movie->CreateArray(&numbers);
		const auto count = a_numbers.GetArraySize() / kNumbersOfAnEye * kNumbersOfAnEye;
		for (std::uint32_t at = 0; at < count; ++at) {
			RE::GFxValue number;
			a_numbers.GetElement(at, &number);
			numbers.PushBack(RE::GFxValue(number.IsNumber() ? number.GetNumber() : 0.0));
		}
		std::array arguments{ numbers, RE::GFxValue(static_cast<double>(Scene::Aspect())) };
		IR_SCRIPT_QUIET("_global.Eyes.draw");
		movie->Invoke("_global.Eyes.draw", nullptr, arguments.data(), static_cast<std::uint32_t>(arguments.size()));
		shown = count > 0;
		return true;
	}

	bool ShowMenu(const RE::GFxValue& a_told)
	{
		if (!a_told.IsArray() || !Load()) {
			return false;
		}
		// the words are made anew in this movie, a string of another one is that one's to keep
		RE::GFxValue told;
		movie->CreateArray(&told);
		for (std::uint32_t at = 0; at < a_told.GetArraySize(); ++at) {
			RE::GFxValue value;
			a_told.GetElement(at, &value);
			if (value.IsString()) {
				RE::GFxValue words;
				movie->CreateString(&words, value.GetString());
				told.PushBack(words);
			} else {
				told.PushBack(RE::GFxValue(value.IsNumber() ? value.GetNumber() : 0.0));
			}
		}
		std::array arguments{ told, RE::GFxValue(static_cast<double>(Scene::Aspect())) };
		IR_SCRIPT_QUIET("_global.Eyes.menu");
		if (!movie->Invoke("_global.Eyes.menu", nullptr, arguments.data(), static_cast<std::uint32_t>(arguments.size()))) {
			return false;
		}
		menuShown = told.GetArraySize() > 0;
		return true;
	}

	void Display()
	{
		if (movie && (shown || menuShown)) {
			movie->Advance(0.0F);
			movie->Display();
		}
	}

	void Clear()
	{
		if (movie && (shown || menuShown)) {
			RE::GFxValue none;
			movie->CreateArray(&none);
			Show(none);
			ShowMenu(none);
		}
	}

	// the inventory and a container draw them in the frame of the panel
	void Install()
	{
		PostDisplay<RE::BarterMenu>::Install();
	}
}
