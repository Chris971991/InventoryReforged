#include "PCH.h"

#include "Companions.h"

#include "Settings.h"
#include "Subject.h"

namespace Companions
{
	namespace
	{
		constexpr std::size_t kMost = 8;

		// the menu a switch shut and what opens once it's gone, the inventory when there's no companion to trade with
		struct Pending
		{
			std::string closing;
			RE::ActorHandle companion;
			bool inventory = false;
		};

		std::mutex pendingLock;
		std::optional<Pending> pending;

		RE::Actor* Teammate(RE::Actor* a_actor)
		{
			return a_actor && a_actor->IsPlayerTeammate() && Subject::Shows(*a_actor) ? a_actor : nullptr;
		}

		void Open(const Pending& a_next)
		{
			if (a_next.inventory) {
				IR_BRACKET("companions: the inventory is asked to open");
				RE::UIMessageQueue::GetSingleton()->AddMessage(RE::InventoryMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
				return;
			}
			// the companion may have walked off or died while the menu shut
			const auto companion = a_next.companion.get();
			if (Teammate(companion.get())) {
				IR_BRACKET("companions: the trade with {:08X} opens", companion->GetFormID());
				RE::ContainerMenu::OpenMenu(companion.get(), RE::ContainerMenu::ContainerMode::kNPCMode);
			} else {
				SKSE::log::info("companions: the one to trade with is gone, nothing opens");
			}
		}
	}

	std::vector<Companion> Near()
	{
		std::vector<std::pair<float, Companion>> found;
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* lists = RE::ProcessLists::GetSingleton();
		if (!player || !lists || !Settings::Get().companionTabs) {
			return {};
		}
		for (const auto& handle : lists->highActorHandles) {
			const auto actor = handle.get();
			if (!Teammate(actor.get())) {
				continue;
			}
			const char* name = actor->GetDisplayFullName();
			found.push_back({ player->GetPosition().GetDistance(actor->GetPosition()), { actor->GetFormID(), name ? name : "" } });
		}
		std::ranges::sort(found, [](const auto& a_one, const auto& a_other) { return a_one.first < a_other.first; });
		std::vector<Companion> companions;
		for (const auto& [distance, companion] : found) {
			if (companions.size() == kMost) {
				break;
			}
			companions.push_back(companion);
		}
		return companions;
	}

	bool SwitchTo(RE::FormID a_actor, const char* a_from)
	{
		auto* ui = RE::UI::GetSingleton();
		if (!ui || !a_from || !ui->IsMenuOpen(a_from) || !Settings::Get().companionTabs) {
			IR_LOG("SwitchTo refused: {}", !ui || !a_from || !ui->IsMenuOpen(a_from) ? "the menu to switch from isn't open" : "the companion tabs are off");
			return false;
		}
		// An inventory a mod opened over a trade goes back to that trade by shutting. Any other switch from there would
		// open a second trade over the first one
		if (a_from == RE::InventoryMenu::MENU_NAME && ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
			RE::NiPointer<RE::TESObjectREFR> traded;
			RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle(), traded);
			if (!traded || traded->GetFormID() != a_actor) {
				IR_LOG("SwitchTo refused: an inventory over the trade with {:08X} goes back to that trade only", traded ? traded->GetFormID() : 0);
				return false;
			}
			IR_BRACKET("companions: the inventory over the trade shuts");
			RE::UIMessageQueue::GetSingleton()->AddMessage(a_from, RE::UI_MESSAGE_TYPE::kHide, nullptr);
			return true;
		}
		Pending next{ a_from, {}, a_actor == 0 };
		if (a_actor != 0) {
			auto* companion = Teammate(RE::TESForm::LookupByID<RE::Actor>(a_actor));
			if (!companion) {
				IR_LOG("SwitchTo refused: {:08X} is no companion the panel can show now", a_actor);
				return false;
			}
			next.companion = companion->GetHandle();
		}
		{
			const std::scoped_lock lock{ pendingLock };
			pending = next;
		}
		SKSE::log::info("companions: {} shuts for {}", a_from, a_actor == 0 ? std::string("the inventory") : fmt::format("the trade with {:08X}", a_actor));
		IR_BRACKET("companions: {} is asked to shut", a_from);
		RE::UIMessageQueue::GetSingleton()->AddMessage(a_from, RE::UI_MESSAGE_TYPE::kHide, nullptr);
		return true;
	}

	void MenuClosed(const RE::BSFixedString& a_menu)
	{
		Pending next;
		{
			const std::scoped_lock lock{ pendingLock };
			if (!pending || pending->closing != a_menu.c_str()) {
				return;
			}
			next = *pending;
			pending.reset();
		}
		SKSE::GetTaskInterface()->AddTask([next] { Open(next); });
	}
}
