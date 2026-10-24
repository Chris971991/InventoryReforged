#include "PCH.h"

#include "StillCamera.h"

#include "Settings.h"

namespace StillCamera
{
	namespace
	{
		bool inventoryOpen = false;
		bool told = false;

		class MenuWatch : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && a_event->menuName == RE::InventoryMenu::MENU_NAME) {
					inventoryOpen = a_event->opening;
					told = false;
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// Souls RE writes its turn at the end of the input pass of PlayerControls (function 42338 of 1.7.104) and
		// the camera reads it later in the frame. This sink is added after PlayerControls, so it runs after it
		class LookWatch : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const*, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				const auto& settings = Settings::Get();
				auto* controls = inventoryOpen && settings.panel && settings.stillCamera ? RE::PlayerControls::GetSingleton() : nullptr;
				if (!controls) {
					return RE::BSEventNotifyControl::kContinue;
				}
				auto& look = controls->data.lookInputVec;
				if (!told && (look.x != 0.0F || look.y != 0.0F)) {
					told = true;
					logger::info("the camera is held still in the inventory, a turn of {:.2f}, {:.2f} was dropped", look.x, look.y);
				}
				look = {};
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Listen()
	{
		static MenuWatch menus;
		RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&menus);
		static LookWatch looks;
		RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&looks);
	}
}
