#include "PCH.h"

#include "Junk.h"

#include <unordered_set>

namespace Junk
{
	namespace
	{
		constexpr std::uint32_t kRecord = 'JUNK';
		constexpr std::uint32_t kVersion = 1;

		std::mutex guard;
		std::unordered_set<RE::FormID> marked;
	}

	bool Has(RE::FormID a_form)
	{
		const std::scoped_lock lock{ guard };
		return marked.contains(a_form);
	}

	bool Set(RE::FormID a_form, bool a_junk)
	{
		if (!RE::TESForm::LookupByID(a_form)) {
			IR_LOG("SetJunk refused: no form {:08X}", a_form);
			return false;
		}
		const std::scoped_lock lock{ guard };
		if (a_junk) {
			marked.insert(a_form);
		} else {
			marked.erase(a_form);
		}
		logger::info("{:08X} {} junk, {} marked", a_form, a_junk ? "is" : "is no", marked.size());
		return true;
	}

	void Save(SKSE::SerializationInterface* a_save)
	{
		const std::scoped_lock lock{ guard };
		if (!a_save->OpenRecord(kRecord, kVersion)) {
			logger::error("the junk marks could not be saved");
			return;
		}
		a_save->WriteRecordData(static_cast<std::uint32_t>(marked.size()));
		for (const auto form : marked) {
			a_save->WriteRecordData(form);
		}
	}

	void Read(SKSE::SerializationInterface* a_save, std::uint32_t a_version)
	{
		if (a_version != kVersion) {
			return;
		}
		const std::scoped_lock lock{ guard };
		marked.clear();
		std::uint32_t count = 0;
		const bool counted = a_save->ReadRecordData(count) > 0;
		// a record cut short ends the reading, the count of a broken one could be anything
		for (std::uint32_t read = 0; counted && read < count; ++read) {
			RE::FormID saved = 0, now = 0;
			if (!a_save->ReadRecordData(saved)) {
				break;
			}
			if (a_save->ResolveFormID(saved, now)) {
				marked.insert(now);
			}
		}
		logger::info("{} junk marks read from the save", marked.size());
	}

	void Forget()
	{
		const std::scoped_lock lock{ guard };
		marked.clear();
	}
}
