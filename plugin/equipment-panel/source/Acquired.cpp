#include "PCH.h"

#include "Acquired.h"

namespace Acquired
{
	namespace
	{
		constexpr std::uint32_t kRecord = 'ACQD';
		// 2 keeps the number the inventory had seen up to
		constexpr std::uint32_t kVersion = 2;
		constexpr RE::FormID kPlayer = 0x14;

		std::mutex guard;
		std::unordered_map<RE::FormID, std::uint32_t> order;
		std::uint32_t last = 0;
		std::uint32_t seen = 0;

		class Gains : public RE::BSTEventSink<RE::TESContainerChangedEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* a_event, RE::BSTEventSource<RE::TESContainerChangedEvent>*) override
			{
				if (a_event && a_event->newContainer == kPlayer && a_event->baseObj != 0) {
					const std::scoped_lock lock{ guard };
					order[a_event->baseObj] = ++last;
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Listen()
	{
		static Gains gains;
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESContainerChangedEvent>(&gains);
	}

	std::uint32_t NumberOf(RE::FormID a_form)
	{
		const std::scoped_lock lock{ guard };
		const auto found = order.find(a_form);
		return found == order.end() ? 0 : found->second;
	}

	bool IsNew(std::uint32_t a_number)
	{
		const std::scoped_lock lock{ guard };
		return a_number > seen;
	}

	void Seen()
	{
		const std::scoped_lock lock{ guard };
		seen = last;
	}

	void Save(SKSE::SerializationInterface* a_save)
	{
		const std::scoped_lock lock{ guard };
		if (!a_save->OpenRecord(kRecord, kVersion)) {
			logger::error("the order of gained items could not be saved");
			return;
		}
		a_save->WriteRecordData(last);
		a_save->WriteRecordData(static_cast<std::uint32_t>(order.size()));
		for (const auto& [form, number] : order) {
			a_save->WriteRecordData(form);
			a_save->WriteRecordData(number);
		}
		a_save->WriteRecordData(seen);
	}

	void Read(SKSE::SerializationInterface* a_save, std::uint32_t a_version)
	{
		if (a_version != 1 && a_version != kVersion) {
			return;
		}
		const std::scoped_lock lock{ guard };
		order.clear();
		std::uint32_t count = 0;
		a_save->ReadRecordData(last);
		const bool counted = a_save->ReadRecordData(count) > 0;
		// a record cut short ends the reading, the count of a broken one could be anything
		for (std::uint32_t read = 0; counted && read < count; ++read) {
			RE::FormID saved = 0, now = 0;
			std::uint32_t number = 0;
			if (!a_save->ReadRecordData(saved) || !a_save->ReadRecordData(number)) {
				break;
			}
			// a plugin may stand elsewhere in the load order than when the game was saved
			if (a_save->ResolveFormID(saved, now)) {
				order[now] = number;
			}
		}
		// a save from before marks nothing new
		seen = last;
		if (a_version >= 2) {
			a_save->ReadRecordData(seen);
		}
		logger::info("the order of {} gained items is read from the save, the last number is {}, seen up to {}", order.size(), last, seen);
	}

	void Forget()
	{
		const std::scoped_lock lock{ guard };
		order.clear();
		last = 0;
		seen = 0;
	}
}
