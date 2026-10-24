#include "PCH.h"

#include "Api.h"

#include "Settings.h"
#include "Subject.h"

namespace Api
{
	namespace
	{
		constexpr auto kScript = "InventoryReforged";
		constexpr auto kWearEvent = "OnInventoryReforgedWear";
		constexpr auto kDoneEvent = "OnInventoryReforgedDone";
		// A framework listens with a quest or a few. A script registering every actor it places would pin them all in
		// the save and get thousands of events on each wear, so each plugin file gets this many and says so. Forms
		// made while the game runs share one file of their own
		constexpr std::size_t kMostOfOneFile = 16;
		constexpr auto kMadeInGame = "made in the game";

		// The scripts that listen for what the panel puts on and takes off, by the file of the form they sit on, kept
		// in the save. The set SKSE gives for this reads its handles with no lock while Papyrus may register at that
		// moment from another thread
		std::mutex receiversLock;
		std::map<RE::VMHandle, std::string> receivers;
		std::set<std::string> toldFull;

		// The last file that touches the form: a mod that puts its script on a quest or a follower of the game overrides
		// that form, counted by Skyrim.esm one mod's sixteen followers would take every other mod's place
		std::string FileOf(const RE::TESForm& a_form)
		{
			const auto* file = a_form.GetFile(-1);
			return file ? std::string(file->GetFilename()) : std::string(kMadeInGame);
		}

		// takes a listener unless its file has all its places, under receiversLock
		bool Take(RE::VMHandle a_handle, const std::string& a_file)
		{
			const auto ofFile = std::ranges::count_if(receivers, [&](const auto& a_receiver) { return a_receiver.second == a_file; });
			if (static_cast<std::size_t>(ofFile) >= kMostOfOneFile) {
				if (toldFull.insert(a_file).second) {
					logger::warn("RegisterForWear: {} forms of {} listen already, more of it are not taken", kMostOfOneFile, a_file);
				}
				return false;
			}
			receivers.emplace(a_handle, a_file);
			return true;
		}

		RE::BSScript::IObjectHandlePolicy* Policy()
		{
			auto* machine = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			return machine ? machine->GetObjectHandlePolicy() : nullptr;
		}

		std::int32_t GetApiVersion(RE::StaticFunctionTag*)
		{
			return kVersion;
		}

		// the one the panel stands for now: the player in the inventory, a follower in their trade, None with no
		// panel up
		RE::Actor* GetPanelActor(RE::StaticFunctionTag*)
		{
			auto* ui = RE::UI::GetSingleton();
			if (!ui || !Settings::Get().panel) {
				return nullptr;
			}
			if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
				return RE::PlayerCharacter::GetSingleton();
			}
			const auto shown = Subject::Get();
			return shown && !shown->IsPlayerRef() ? shown.get() : nullptr;
		}

		bool ShowsPanelFor(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return a_actor && Subject::Shows(*a_actor);
		}

		void RegisterForWear(RE::StaticFunctionTag*, RE::TESForm* a_receiver)
		{
			auto* policy = Policy();
			if (!a_receiver || !policy) {
				return;
			}
			const auto handle = policy->GetHandleForObject(static_cast<RE::VMTypeID>(a_receiver->GetFormType()), a_receiver);
			if (handle == policy->EmptyHandle()) {
				return;
			}
			const auto file = FileOf(*a_receiver);
			const std::scoped_lock lock{ receiversLock };
			if (!receivers.contains(handle) && Take(handle, file)) {
				policy->PersistHandle(handle);
				IR_LOG("RegisterForWear: {:08X} of {} listens now", a_receiver->GetFormID(), file);
			}
		}

		void SetReadOnly(RE::StaticFunctionTag*, RE::Actor* a_actor, bool a_readOnly)
		{
			IR_LOG("SetReadOnly: a script asks {:08X} be {}", a_actor ? a_actor->GetFormID() : 0, a_readOnly ? "looked at only" : "dressed again");
			if (a_actor && !a_actor->IsPlayerRef()) {
				Subject::SetReadOnly(a_actor->GetFormID(), a_readOnly);
			}
		}

		std::vector<RE::VMHandle> Receivers()
		{
			const std::scoped_lock lock{ receiversLock };
			std::vector<RE::VMHandle> handles;
			for (const auto& [handle, file] : receivers) {
				handles.push_back(handle);
			}
			return handles;
		}

		void UnregisterForWear(RE::StaticFunctionTag*, RE::TESForm* a_receiver)
		{
			auto* policy = Policy();
			if (!a_receiver || !policy) {
				return;
			}
			const auto handle = policy->GetHandleForObject(static_cast<RE::VMTypeID>(a_receiver->GetFormType()), a_receiver);
			const std::scoped_lock lock{ receiversLock };
			if (receivers.erase(handle) > 0) {
				policy->ReleaseHandle(handle);
				IR_LOG("UnregisterForWear: {:08X} listens no more", a_receiver->GetFormID());
			}
		}
	}

	void Bind(RE::BSScript::IVirtualMachine& a_machine)
	{
		a_machine.RegisterFunction("GetApiVersion", kScript, GetApiVersion, true);
		a_machine.RegisterFunction("GetPanelActor", kScript, GetPanelActor);
		a_machine.RegisterFunction("ShowsPanelFor", kScript, ShowsPanelFor);
		a_machine.RegisterFunction("RegisterForWear", kScript, RegisterForWear);
		a_machine.RegisterFunction("UnregisterForWear", kScript, UnregisterForWear);
		a_machine.RegisterFunction("SetReadOnly", kScript, SetReadOnly);
	}


	void Worn(RE::Actor& a_wearer, RE::TESForm& a_item, bool a_on)
	{
		auto* machine = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!machine) {
			return;
		}
		const RE::BSFixedString name{ kWearEvent };
		const auto listeners = Receivers();
		IR_LOG("{} goes to {} listeners: {:08X} {} {:08X}", kWearEvent, listeners.size(), a_wearer.GetFormID(), a_on ? "put on" : "took off", a_item.GetFormID());
		for (const auto handle : listeners) {
			auto* arguments = RE::MakeFunctionArguments(&a_wearer, &a_item, std::move(a_on));
			machine->SendEvent(handle, name, arguments);
		}
	}

	void Done(RE::Actor& a_wearer, bool a_changed)
	{
		auto* machine = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!machine) {
			return;
		}
		const RE::BSFixedString name{ kDoneEvent };
		const auto listeners = Receivers();
		IR_LOG("{} goes to {} listeners: the trade with {:08X} is over, {}", kDoneEvent, listeners.size(), a_wearer.GetFormID(), a_changed ? "it changed gear" : "nothing changed");
		for (const auto handle : listeners) {
			auto* arguments = RE::MakeFunctionArguments(&a_wearer, std::move(a_changed));
			machine->SendEvent(handle, name, arguments);
		}
	}

	// a listener deleted from the game since it registered goes out of the set, it would be kept in every save
	void Save(SKSE::SerializationInterface* a_save)
	{
		auto* policy = Policy();
		{
			const std::scoped_lock lock{ receiversLock };
			// only a form made in the game can be gone for good, one of a plugin may sit in a cell unloaded now. A
			// handle keeps the form id in its low half
			for (auto receiver = receivers.begin(); policy && receiver != receivers.end();) {
				const auto form = static_cast<RE::FormID>(receiver->first & 0xFFFFFFFF);
				if ((form >> 24) != 0xFF || policy->IsHandleObjectAvailable(receiver->first)) {
					++receiver;
				} else {
					policy->ReleaseHandle(receiver->first);
					receiver = receivers.erase(receiver);
				}
			}
			// each listener with the name of its file, the places of a file are counted again after a load
			if (!receivers.empty() && a_save->OpenRecord(kRecord, 1)) {
				a_save->WriteRecordData(static_cast<std::uint32_t>(receivers.size()));
				for (const auto& [handle, file] : receivers) {
					const auto length = static_cast<std::uint16_t>(std::min<std::size_t>(file.size(), 0xFFFF));
					a_save->WriteRecordData(handle);
					a_save->WriteRecordData(length);
					a_save->WriteRecordData(file.data(), length);
				}
			}
		}
		const auto readOnly = Subject::ReadOnlyActors();
		if (!readOnly.empty() && a_save->OpenRecord(kReadOnlyRecord, 1)) {
			a_save->WriteRecordData(static_cast<std::uint32_t>(readOnly.size()));
			for (const auto form : readOnly) {
				a_save->WriteRecordData(form);
			}
		}
	}

	void Load(SKSE::SerializationInterface* a_save, std::uint32_t a_record)
	{
		std::uint32_t count = 0;
		if (!a_save->ReadRecordData(count)) {
			return;
		}
		if (a_record == kReadOnlyRecord) {
			for (std::uint32_t at = 0; at < count; ++at) {
				RE::FormID form = 0;
				// a record cut short ends the reading, the count of a broken one could be anything
				if (!a_save->ReadRecordData(form)) {
					break;
				}
				if (a_save->ResolveFormID(form, form)) {
					Subject::SetReadOnly(form, true);
				}
			}
			return;
		}
		// as the set SKSE gives does: the handles kept in the save are bound again by the game, a revert lets them go
		const std::scoped_lock lock{ receiversLock };
		for (std::uint32_t at = 0; at < count; ++at) {
			RE::VMHandle handle = 0;
			std::uint16_t length = 0;
			if (!a_save->ReadRecordData(handle) || !a_save->ReadRecordData(length)) {
				return;
			}
			std::string file(length, '\0');
			if (length > 0 && a_save->ReadRecordData(file.data(), length) != length) {
				return;
			}
			if (a_save->ResolveHandle(handle, handle) && !receivers.contains(handle)) {
				Take(handle, file);
			}
		}
	}

	void Revert()
	{
		auto* policy = Policy();
		{
			const std::scoped_lock lock{ receiversLock };
			if (policy) {
				for (const auto& [handle, file] : receivers) {
					policy->ReleaseHandle(handle);
				}
			}
			receivers.clear();
			toldFull.clear();
		}
		Subject::ForgetReadOnly();
	}
}
