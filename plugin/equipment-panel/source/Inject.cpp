#include "PCH.h"

#include "Inject.h"

#include "Profile.h"
#include "Settings.h"
#include "Subject.h"

namespace Inject
{
	namespace
	{
		constexpr auto kScript = "InventoryReforged/panel.swf";
		constexpr auto kHolder = "equipmentPanelScript";
		struct Menu
		{
			std::string_view file;
			const char* kind;
			std::string_view name;
		};

		constexpr std::array kMenus{
			Menu{ "inventorymenu.swf"sv, "inventory", RE::InventoryMenu::MENU_NAME },
			Menu{ "containermenu.swf"sv, "container", RE::ContainerMenu::MENU_NAME },
			Menu{ "bartermenu.swf"sv, "barter", RE::BarterMenu::MENU_NAME },
			Menu{ "magicmenu.swf"sv, "magic", RE::MagicMenu::MENU_NAME },
		};

		std::string Lower(std::string_view a_text)
		{
			std::string lower(a_text);
			std::ranges::transform(lower, lower.begin(), [](unsigned char a_char) { return static_cast<char>(std::tolower(a_char)); });
			return lower;
		}

		const Menu* MenuOf(const RE::GFxMovieView& a_view)
		{
			const auto* definition = a_view.GetMovieDef();
			const char* url = definition ? definition->GetFileURL() : nullptr;
			if (!url) {
				return nullptr;
			}
			const auto file = Lower(url);
			const auto menu = std::ranges::find_if(kMenus, [&](const auto& a_menu) { return file.ends_with(a_menu.file); });
			if (menu == kMenus.end() || (menu->name == RE::MagicMenu::MENU_NAME && !Settings::Get().magicMenu)) {
				return nullptr;
			}
			return &*menu;
		}
	}

	bool HoldsPanel(const RE::GFxMovieView& a_view)
	{
		return Settings::Get().panel && MenuOf(a_view) != nullptr;
	}

	void IntoMenu(RE::GFxMovieView& a_view)
	{
		const auto* menu = Settings::Get().panel ? MenuOf(a_view) : nullptr;
		if (!menu) {
			return;
		}
		const char* url = a_view.GetMovieDef()->GetFileURL();

		// the watch looks at the menu from its load on, a freeze while the menu is made is told too
		Profile::MenuMade(menu->name);
#if IR_DEBUG_EDITION
		Debug::Script::MovieMade(a_view, menu->kind);
		const auto* definition = a_view.GetMovieDef();
		logger::info("the menu {} is made from {}: SWF version {}, stage {:.0f} x {:.0f}, {:.0f} frames a second", menu->kind, url, definition->GetVersion(), definition->GetWidth(),
			definition->GetHeight(), definition->GetFrameRate());
#endif

		RE::GFxValue root;
		if (!a_view.GetVariable(&root, "_root") || !root.IsDisplayObject()) {
			logger::warn("no root in {}, the panel stays out", url);
			return;
		}
		a_view.SetVariable("_global.EquipmentPanelMenu", RE::GFxValue(menu->kind));
		// whom the panel shows is settled before the menu fills its rows, a follower's worn rows get their marks then
		Subject::Ask(std::string_view(menu->kind) == "container");
		RE::GFxValue holder;
		if (!root.CreateEmptyMovieClip(&holder, kHolder)) {
			logger::warn("no room for the panel in {}", url);
			return;
		}
		const RE::GFxValue path(kScript);
		IR_BRACKET("the panel script {} is asked to load", kScript);
		holder.Invoke("loadMovie", nullptr, &path, 1);
		logger::info("the panel script is loading into {}", url);
	}
}
