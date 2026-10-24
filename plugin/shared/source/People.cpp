#include "PCH.h"

#include "People.h"

namespace People
{
	namespace
	{
		Lookalikes lookalikes = nullptr;
	}

	const RE::TESNPC& TraitsOf(const RE::TESNPC& a_person)
	{
		using Use = RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG;
		constexpr int kDeepest = 6;
		const auto firstPerson = [](const RE::TESLevCharacter& a_list) -> const RE::TESNPC* {
			for (const auto& entry : a_list.entries) {
				if (const auto* one = entry.form ? entry.form->As<RE::TESNPC>() : nullptr) {
					return one;
				}
			}
			return nullptr;
		};
		// the flags of the template count wherever the record names a template, the game's own "uses template" bit
		// isn't set on many of them (Kornalus)
		const RE::TESNPC* at = &a_person;
		for (int depth = 0; depth < kDeepest && at->baseTemplateForm && at->actorData.templateUseFlags.any(Use::kTraits); ++depth) {
			const auto* next = at->baseTemplateForm->As<RE::TESNPC>();
			if (const auto* list = next ? nullptr : at->baseTemplateForm->As<RE::TESLevCharacter>()) {
				next = firstPerson(*list);
			}
			if (!next) {
				break;
			}
			at = next;
		}
		return *at;
	}

	const RE::TESRace* RaceOf(const RE::TESNPC& a_person)
	{
		return TraitsOf(a_person).race;
	}

	void SetLookalikes(Lookalikes a_find)
	{
		lookalikes = a_find;
	}

	std::span<RE::TESNPC* const> LookalikesOf(const RE::TESNPC& a_person)
	{
		return lookalikes ? lookalikes(a_person) : std::span<RE::TESNPC* const>();
	}
}
