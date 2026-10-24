#include "PCH.h"

#include "Hidden.h"

#include "Keep.h"
#include "PlayerView.h"
#include "Profile.h"
#include "Quests.h"
#include "Scene.h"
#include "Settings.h"
#include "Subject.h"

namespace Hidden
{
	namespace
	{
		constexpr std::uint32_t kRecord = 'HIDE';
		// 1 had the slots only, 2 adds the single pieces, 3 the wigs pushed off
		constexpr std::uint32_t kVersion = 3;
		// the visitor keeps the actor it builds for here, the call it makes for every worn item passes it on
		constexpr std::size_t kVisitorActor = 0x10;
		constexpr std::size_t kVisit = 1;

		std::atomic<std::uint32_t> mask{ 0 };

		// the body is built off the main thread too, so the list sits behind a lock and a flag says whether it's empty
		std::mutex piecesLock;
		std::vector<RE::FormID> pieces;
		std::atomic<bool> anyPieces{ false };

		bool PieceHidden(RE::FormID a_form)
		{
			if (!anyPieces.load()) {
				return false;
			}
			const std::scoped_lock hold{ piecesLock };
			return std::ranges::find(pieces, a_form) != pieces.end();
		}

		void StorePieces(std::vector<RE::FormID> a_pieces)
		{
			const std::scoped_lock hold{ piecesLock };
			pieces = std::move(a_pieces);
			anyPieces = !pieces.empty();
		}

		// the hooks cost a flag while nothing is hidden
		bool AnythingHidden()
		{
			return mask.load() != 0 || anyPieces.load();
		}

		// the slot a piece of armor shows in, the same order the panel script uses
		std::int32_t SlotOf(RE::TESObjectARMO& a_armor)
		{
			const auto parts = a_armor.GetSlotMask().underlying();
			constexpr std::uint32_t kShield = 1 << 9;
			if (parts & kShield) {
				return kLeft;
			}
			constexpr std::array kInOrder{
				std::pair{ kBody, 0x4U },
				std::pair{ kHead, 0x3803U },
				std::pair{ kHands, 0x18U },
				std::pair{ kFeet, 0x180U },
				std::pair{ kAmulet, 0x20U },
				std::pair{ kRing, 0x40U },
			};
			for (const auto& [slot, bits] : kInOrder) {
				if (parts & bits) {
					return slot;
				}
			}
			// A piece with no body slot at all, jewelry MARA made slotless while the game runs, goes by its keywords.
			// One with slots of its own keeps to them, cloak mods give cloaks the necklace keyword for enchanting
			constexpr std::array kByWord{
				std::pair{ kRing, "ClothingRing"sv },
				std::pair{ kRing, "gearMaraRing"sv },
				std::pair{ kAmulet, "ClothingNecklace"sv },
				std::pair{ kAmulet, "gearMaraAmulet"sv },
			};
			for (const auto& [slot, word] : kByWord) {
				if (parts == 0 && a_armor.HasKeywordString(word)) {
					return slot;
				}
			}
			return kOther;
		}

		template <class Hides>
		bool SkippedBy(RE::TESBoundObject* a_object, std::uint32_t a_mask, Hides&& a_pieceHidden)
		{
			// with the panel off there is no eye to show a slot again, so nothing stays hidden
			if (!a_object || !Settings::Get().panel) {
				return false;
			}
			std::int32_t slot = -1;
			if (auto* armor = a_object->As<RE::TESObjectARMO>()) {
				// mods put pieces the menu never lists onto the body (body parts, devices, tokens), those stay
				if ((armor->formFlags & RE::TESObjectARMO::RecordFlags::kNonPlayable) != 0) {
					return false;
				}
				if (a_pieceHidden(armor->GetFormID())) {
					return true;
				}
				slot = SlotOf(*armor);
			} else if (a_object->Is(RE::FormType::Ammo)) {
				slot = kAmmo;
			}
			return slot >= 0 && (a_mask & (1U << slot)) != 0;
		}

		bool Skipped(RE::TESBoundObject* a_object)
		{
			return SkippedBy(a_object, mask.load(), [](RE::FormID a_form) { return PieceHidden(a_form); });
		}

		// What a follower has hidden, kept per actor by form id. Their body is built from the same visitor as the
		// player's, it leaves these pieces out
		struct Kept
		{
			std::uint32_t mask = 0;
			std::vector<RE::FormID> pieces;

			bool Empty() const { return mask == 0 && pieces.empty(); }
			bool Hides(RE::FormID a_form) const { return std::ranges::find(pieces, a_form) != pieces.end(); }
		};
		constexpr std::uint32_t kFollowersVersion = 1;
		constexpr std::size_t kMostFollowers = 64;

		std::mutex followersLock;
		std::unordered_map<RE::FormID, Kept> followers;
		std::atomic<bool> anyFollowers{ false };

		std::optional<Kept> KeptOf(const RE::Actor& a_actor)
		{
			if (!anyFollowers.load()) {
				return std::nullopt;
			}
			const std::scoped_lock hold{ followersLock };
			const auto found = followers.find(a_actor.GetFormID());
			return found != followers.end() ? std::optional<Kept>{ found->second } : std::nullopt;
		}

		bool FollowerSkips(const RE::Actor& a_actor, RE::TESBoundObject* a_object)
		{
			if (!anyFollowers.load()) {
				return false;
			}
			const std::scoped_lock hold{ followersLock };
			const auto found = followers.find(a_actor.GetFormID());
			return found != followers.end() && SkippedBy(a_object, found->second.mask, [&](RE::FormID a_form) { return found->second.Hides(a_form); });
		}

		// Followers whose body is to be built again once the item menus are closed, and the pieces their own AI put on
		// while something of theirs is hidden: a newer piece in a hidden slot pushes the older one off, the game looks
		// for that clash among the pieces on the body only. Asked from events of any thread
		struct PutOnBy
		{
			RE::FormID actor;
			RE::FormID armor;
		};
		std::mutex followerOwedLock;
		std::vector<RE::FormID> followerRebuilds;
		std::vector<PutOnBy> followerPutOn;
		std::atomic<bool> followerOwed{ false };

		void OweRebuild(RE::FormID a_actor)
		{
			const std::scoped_lock hold{ followerOwedLock };
			if (std::ranges::find(followerRebuilds, a_actor) == followerRebuilds.end()) {
				followerRebuilds.push_back(a_actor);
			}
			followerOwed = true;
		}

		// TNG keeps the genitals off under covering armor with a blank piece of its own in slot 52, put on and taken
		// off on equip events by the keywords of what is worn. A hidden piece stays worn for it, so that blank piece
		// stayed on and the body lost what a plain take-off shows
		struct Gentleman
		{
			RE::TESObjectARMO* cover = nullptr;
			RE::BGSKeyword* covering = nullptr;
			RE::BGSKeyword* revealingOnlyWomen = nullptr;
			RE::BGSKeyword* revealingOnlyMen = nullptr;
		};
		Gentleman tng;

		// the same keyword rule TNG uses to put its cover on
		bool CoversGenitals(RE::TESObjectARMO& a_armor, bool a_female)
		{
			return (tng.covering && a_armor.HasKeyword(tng.covering)) || (!a_female && tng.revealingOnlyWomen && a_armor.HasKeyword(tng.revealingOnlyWomen)) ||
				(a_female && tng.revealingOnlyMen && a_armor.HasKeyword(tng.revealingOnlyMen));
		}

		std::atomic<bool> rebuildOwed{ false };
		// grows whenever what is worn or hidden may have changed
		std::atomic<std::uint32_t> wornChanges{ 0 };

		// Wig mods make hair a piece of armor on the hair slots. A helmet takes those slots too and the game takes
		// the wig off, the hidden helmet stays worn, so the head went bald. Here a wig some other piece pushed off is
		// kept, to be drawn on the body in that piece's place while it's hidden
		struct Push
		{
			RE::FormID by;
			RE::FormID off;
		};

		// hair and long hair
		constexpr std::uint32_t kHairSlots = (1U << 1) | (1U << 11);
		// the rest of the game's own slots but the tail: head, circlet and ears, body, hands, feet, jewelry, shield
		constexpr std::uint32_t kGearSlots = 0x3FFFU & ~kHairSlots & ~(1U << 10);

		// any piece on the hair that takes no headgear and nothing of the body, whatever mod slots it holds besides
		bool IsWig(std::uint32_t a_parts)
		{
			return (a_parts & kHairSlots) != 0 && (a_parts & kGearSlots) == 0;
		}

		// both events of one push come from one equip
		constexpr auto kPushTime = 250ms;
		constexpr std::size_t kPushesKept = 16;

		struct Change
		{
			RE::FormID form = 0;
			std::uint32_t parts = 0;
			std::chrono::steady_clock::time_point at;
		};

		std::mutex pushesLock;
		std::vector<Push> pushes;
		Change lastOn;
		Change lastOff;

		std::vector<Push> Pushes()
		{
			const std::scoped_lock hold{ pushesLock };
			return pushes;
		}

		// A wig that goes off as another piece goes onto its slots, in either order, was pushed off by it. A wig
		// put over a wig pushes nothing, that one is just changed. Gives the piece that pushed, 0 when none did
		RE::FormID Remember(const RE::TESEquipEvent& a_event, const RE::TESObjectARMO& a_armor)
		{
			const auto now = std::chrono::steady_clock::now();
			const Change change{ a_armor.GetFormID(), a_armor.GetSlotMask().underlying(), now };
			const auto justNow = [&](const Change& a_other) {
				return a_other.form != 0 && a_other.form != change.form && (a_other.parts & change.parts) != 0 && now - a_other.at < kPushTime;
			};
			const std::scoped_lock hold{ pushesLock };
			std::optional<Push> pushed;
			if (a_event.equipped) {
				std::erase_if(pushes, [&](const Push& a_push) { return a_push.off == change.form; });
				if (!IsWig(change.parts) && justNow(lastOff)) {
					pushed = Push{ change.form, lastOff.form };
				}
				lastOn = change;
			} else {
				std::erase_if(pushes, [&](const Push& a_push) { return a_push.by == change.form; });
				if (IsWig(change.parts)) {
					if (justNow(lastOn) && !IsWig(lastOn.parts)) {
						pushed = Push{ lastOn.form, change.form };
					}
					lastOff = change;
				}
			}
			if (!pushed || std::ranges::any_of(pushes, [&](const Push& a_push) { return a_push.by == pushed->by && a_push.off == pushed->off; })) {
				return 0;
			}
			if (pushes.size() >= kPushesKept) {
				pushes.erase(pushes.begin());
			}
			pushes.push_back(*pushed);
			const auto gap = std::chrono::duration_cast<std::chrono::milliseconds>(now - (a_event.equipped ? lastOff.at : lastOn.at)).count();
			logger::info("wig {:08X} was pushed off by {:08X} ({} ms apart), it goes on the body while that one is hidden", pushed->off, pushed->by, gap);
			return pushed->by;
		}

		// the entry of a piece the player has and doesn't wear
		RE::InventoryEntryData* HeldUnworn(RE::PlayerCharacter& a_player, RE::FormID a_form)
		{
			auto* changes = a_player.GetInventoryChanges();
			if (!changes || !changes->entryList) {
				return nullptr;
			}
			for (auto* entry : *changes->entryList) {
				if (entry && entry->object && entry->object->GetFormID() == a_form) {
					return entry->countDelta > 0 && !entry->IsWorn() ? entry : nullptr;
				}
			}
			return nullptr;
		}

		// biped slots worn pieces cover: by shown pieces, by hidden pieces only, and by the wigs drawn in place of
		// hidden pieces, and whether a shown piece covers the genitals for TNG. Worked out again after a change of
		// what is worn or hidden
		struct Covers
		{
			std::uint32_t shown = 0;
			std::uint32_t hidden = 0;
			std::uint32_t standing = 0;
			bool genitals = false;
		};

		// the body can be built off the main thread, which reads this every frame. The wigs drawn in place of
		// hidden pieces come with it when asked for
		Covers CoversOf(RE::PlayerCharacter& a_player, std::vector<Push>* a_standIns = nullptr)
		{
			static std::mutex counting;
			static std::uint32_t countedAt = ~0U;
			static Covers covers;
			static std::vector<Push> standIns;
			const std::scoped_lock lock{ counting };
			if (countedAt == wornChanges.load()) {
				if (a_standIns) {
					*a_standIns = standIns;
				}
				return covers;
			}
			countedAt = wornChanges.load();
			covers = {};
			standIns.clear();
			std::vector<RE::FormID> hiddenWorn;
			const auto* base = a_player.GetActorBase();
			const bool female = base && base->GetSex() == RE::SEX::kFemale;
			auto* changes = a_player.GetInventoryChanges();
			if (changes && changes->entryList) {
				for (auto* entry : *changes->entryList) {
					auto* armor = entry && entry->object ? entry->object->As<RE::TESObjectARMO>() : nullptr;
					if (!armor || !entry->IsWorn()) {
						continue;
					}
					const bool skipped = Skipped(armor);
					(skipped ? covers.hidden : covers.shown) |= armor->GetSlotMask().underlying();
					if (skipped) {
						hiddenWorn.push_back(armor->GetFormID());
					}
					if (!skipped && armor != tng.cover && CoversGenitals(*armor, female)) {
						covers.genitals = true;
					}
				}
			}
			// a wig goes on where nothing shown sits, the one that pushed it is worn and hidden, and the wig is still
			// in the bag and not hidden as a piece of its own
			for (const auto& push : hiddenWorn.empty() ? std::vector<Push>{} : Pushes()) {
				auto* wig = RE::TESForm::LookupByID<RE::TESObjectARMO>(push.off);
				if (!wig || std::ranges::find(hiddenWorn, push.by) == hiddenWorn.end() || PieceHidden(push.off) || !HeldUnworn(a_player, push.off)) {
					continue;
				}
				const auto parts = wig->GetSlotMask().underlying();
				if ((parts & (covers.shown | covers.standing)) == 0) {
					covers.standing |= parts;
					standIns.push_back(push);
				}
			}
			// the game keeps the hair's own part off under a wig, so it stays off under one drawn in place
			covers.hidden &= ~(covers.shown | covers.standing);
			if (a_standIns) {
				*a_standIns = standIns;
			}
			return covers;
		}

		// TNG's cover goes with the hidden pieces when none of the shown ones covers
		bool CoverSkipped(RE::PlayerCharacter& a_player, RE::TESBoundObject* a_object)
		{
			return tng.cover && a_object == tng.cover && AnythingHidden() && !CoversOf(a_player).genitals;
		}

		struct Visit
		{
			static RE::BSContainer::ForEachResult thunk(RE::InventoryChanges::IItemChangeVisitor* a_visitor, RE::InventoryEntryData* a_entry)
			{
				auto* actor = *reinterpret_cast<RE::Actor* const*>(reinterpret_cast<std::uintptr_t>(a_visitor) + kVisitorActor);
				auto* player = RE::PlayerCharacter::GetSingleton();
				IR_HOOK("worn piece put on a body");
				IR_NOTE(irTimer, "actor {:08X}, item {:08X}", actor ? actor->GetFormID() : 0, a_entry && a_entry->object ? a_entry->object->GetFormID() : 0);
				// the doll follows the body of whoever the panel shows, a follower's too
				if (actor == player || (actor && Subject::IsShown(*actor))) {
					PlayerView::BodyRebuilt();
				}
				if (AnythingHidden() && a_entry) {
					if (actor == player && (Skipped(a_entry->object) || CoverSkipped(*player, a_entry->object))) {
						StandIn(a_visitor, *player, a_entry->object);
						return RE::BSContainer::ForEachResult::kContinue;
					}
				}
				if (actor && actor != player && a_entry && FollowerSkips(*actor, a_entry->object)) {
					return RE::BSContainer::ForEachResult::kContinue;
				}
				return original(a_visitor, a_entry);
			}

			// A wig the hidden piece pushed off goes onto the body in its place, down the call the game puts every worn
			// piece on with. For armor that call reads nothing but the form. The wig stays off for the game
			static void StandIn(RE::InventoryChanges::IItemChangeVisitor* a_visitor, RE::PlayerCharacter& a_player, RE::TESBoundObject* a_hidden)
			{
				std::vector<Push> standIns;
				CoversOf(a_player, &standIns);
				for (const auto& push : standIns) {
					if (a_hidden && push.by == a_hidden->GetFormID()) {
						if (auto* wig = HeldUnworn(a_player, push.off)) {
							original(a_visitor, wig);
							logger::info("wig {} goes on the body in place of hidden {}", wig->object->GetName(), a_hidden->GetName());
						}
					}
				}
			}

			static inline REL::Relocation<decltype(thunk)> original;
		};

		std::uint32_t HiddenCover(RE::PlayerCharacter& a_player)
		{
			return CoversOf(a_player).hidden;
		}

		// the biped slot a part of a skin stands for, head parts number theirs from 130 and some hair mods use the
		// numbers of the body from 30. -1 for any other part
		int SlotOfPartition(std::uint16_t a_slot)
		{
			constexpr std::uint16_t kBodySlots = 30;
			constexpr std::uint16_t kHeadPartSlots = 130;
			if (a_slot >= kHeadPartSlots && a_slot < kHeadPartSlots + 32) {
				return a_slot - kHeadPartSlots;
			}
			return a_slot >= kBodySlots && a_slot < kBodySlots + 32 ? a_slot - kBodySlots : -1;
		}

		// The nodes of the player's hair in the face node, named after the head parts: the hair and every extra part
		// it brings. Hair mods with physics (Dint999 and the like) hang most of the hair on extra parts of no kind
		void HairNodes(const RE::BGSHeadPart& a_part, bool a_ofHair, std::vector<std::string>& a_names, int a_depth = 0)
		{
			constexpr int kDeepest = 4;
			const bool hair = a_ofHair || a_part.type.get() == RE::BGSHeadPart::HeadPartType::kHair;
			if (hair && !a_part.formEditorID.empty()) {
				a_names.emplace_back(a_part.formEditorID.c_str());
			}
			for (const auto* extra : a_part.extraParts) {
				if (extra && a_depth < kDeepest) {
					HairNodes(*extra, hair, a_names, a_depth + 1);
				}
			}
		}

		std::vector<std::string> HairNodes(RE::Actor& a_actor)
		{
			std::vector<std::string> names;
			const auto* base = a_actor.GetActorBase();
			for (std::int8_t i = 0; base && base->headParts && i < base->numHeadParts; ++i) {
				if (base->headParts[i]) {
					HairNodes(*base->headParts[i], false, names);
				}
			}
			return names;
		}

		// The game switches head parts off for the slots worn armor covers, by what is worn and not by what is
		// drawn: the whole head node under a full helmet, the hair's own part under a helmet, and the hair's extra
		// parts with no parts of a skin to switch it culls whole. Under hidden pieces all of it goes back on, on the
		// face given (the body's own or a copy's). a_tell writes what came back, a copy's is made too often for that
		void UncoverFace(RE::NiAVObject* a_face, RE::Actor& a_actor, std::uint32_t a_hidden, bool a_tell)
		{
			constexpr std::uint32_t kHeadSlot = 1;
			constexpr std::uint32_t kHairSlot = 1U << 1;
			auto* face = a_face;
			if (!face || a_hidden == 0) {
				return;
			}
			const auto hidden = a_hidden;
			if (face->GetAppCulled() && (hidden & kHeadSlot) != 0) {
				face->SetAppCulled(false);
			}
			const auto hair = (hidden & kHairSlot) != 0 ? HairNodes(a_actor) : std::vector<std::string>{};
			Scene::Visit(*face, [&](RE::NiAVObject& a_part) {
				if (a_part.GetAppCulled() && &a_part != face && std::ranges::find(hair, std::string_view(a_part.name.c_str())) != hair.end()) {
					a_part.SetAppCulled(false);
					if (a_tell) {
						logger::info("hair part {} is shown again under a hidden piece", a_part.name.c_str());
					}
				}
				auto* shape = a_part.AsGeometry();
				auto* skin = shape ? netimmerse_cast<RE::BSDismemberSkinInstance*>(shape->GetGeometryRuntimeData().skinInstance.get()) : nullptr;
				if (!skin) {
					return;
				}
				const auto& parts = skin->GetRuntimeData();
				for (std::int32_t i = 0; i < parts.numPartitions; ++i) {
					const auto slot = SlotOfPartition(parts.partitions[i].slot);
					if (!parts.partitions[i].editorVisible && slot >= 0 && (hidden & (1U << slot)) != 0) {
						skin->UpdateDismemberPartion(parts.partitions[i].slot, true);
						if (a_tell) {
							logger::info("part {} of {} is shown again under a hidden piece", parts.partitions[i].slot, a_part.name.c_str());
						}
					}
				}
			});
		}

		// First person is left alone on the body, camera mods there switch the head off on purpose. The doll's copy
		// gets its face back all the same (UncoverCopiedFace): in first person the head stayed off under a hidden helmet
		void ShowCovered(RE::Actor& a_actor, std::uint32_t a_hidden)
		{
			auto* face = a_hidden != 0 && a_actor.Get3D(false) ? a_actor.GetFaceNodeSkinned() : nullptr;
			const auto* camera = RE::PlayerCamera::GetSingleton();
			if (!face || (a_actor.IsPlayerRef() && camera && camera->IsInFirstPerson())) {
				return;
			}
			UncoverFace(face, a_actor, a_hidden, true);
		}

		void ShowCovered(RE::PlayerCharacter& a_player)
		{
			ShowCovered(a_player, HiddenCover(a_player));
		}

		// the slots a follower's hidden pieces cover and no shown piece does
		std::uint32_t FollowerCover(RE::Actor& a_actor, const Kept& a_kept)
		{
			std::uint32_t shown = 0;
			std::uint32_t hidden = 0;
			auto* changes = a_actor.GetInventoryChanges();
			if (!changes || !changes->entryList) {
				return 0;
			}
			for (auto* entry : *changes->entryList) {
				auto* armor = entry && entry->object ? entry->object->As<RE::TESObjectARMO>() : nullptr;
				if (armor && entry->IsWorn()) {
					const bool skipped = SkippedBy(armor, a_kept.mask, [&](RE::FormID a_form) { return a_kept.Hides(a_form); });
					(skipped ? hidden : shown) |= armor->GetSlotMask().underlying();
				}
			}
			return hidden & ~shown;
		}

		// a piece put on, its copy told apart by the unique id the game gives an enchanted or improved one
		struct PutOn
		{
			RE::FormID form;
			std::uint16_t unique;
		};

		// one worn piece with the data of its own copy
		struct WornCopy
		{
			RE::TESObjectARMO* armor;
			RE::ExtraDataList* data;
			std::uint16_t unique;

			bool operator==(const WornCopy& a_other) const { return data == a_other.data && armor == a_other.armor; }
		};

		std::mutex putOnLock;
		std::vector<PutOn> putOn;

		std::vector<WornCopy> WornCopies(RE::PlayerCharacter& a_player)
		{
			std::vector<WornCopy> worn;
			auto* changes = a_player.GetInventoryChanges();
			if (!changes || !changes->entryList) {
				return worn;
			}
			for (auto* entry : *changes->entryList) {
				auto* armor = entry && entry->object ? entry->object->As<RE::TESObjectARMO>() : nullptr;
				if (!armor || !entry->extraLists || (armor->formFlags & RE::TESObjectARMO::RecordFlags::kNonPlayable) != 0) {
					continue;
				}
				for (auto* data : *entry->extraLists) {
					if (data && (data->HasType(RE::ExtraDataType::kWorn) || data->HasType(RE::ExtraDataType::kWornLeft))) {
						const auto* id = data->GetByType<RE::ExtraUniqueID>();
						worn.push_back({ armor, data, id ? id->uniqueID : std::uint16_t{ 0 } });
					}
				}
			}
			return worn;
		}

		// The game takes a worn piece off when a new one claims its slot, but it looks for that clash among the
		// pieces on the body, and a hidden one isn't there: both stayed worn. Here the newest piece keeps its slots
		// and every other worn copy on them goes, an earlier new one, one hidden then shown again, or a twin of
		// the same armor with other enchantment
		std::vector<WornCopy> Clashing(RE::PlayerCharacter& a_player)
		{
			std::vector<PutOn> added;
			{
				const std::scoped_lock hold{ putOnLock };
				added = putOn;
			}
			const auto worn = WornCopies(a_player);
			std::vector<WornCopy> kept;
			std::vector<WornCopy> off;
			for (auto newest = added.rbegin(); newest != added.rend(); ++newest) {
				const auto copies = std::ranges::count_if(worn, [&](const WornCopy& a_copy) { return a_copy.armor->GetFormID() == newest->form; });
				const auto newcomer = std::ranges::find_if(worn, [&](const WornCopy& a_copy) {
					// with twins worn the copy has to be known by its id, without them the one worn copy is it
					return a_copy.armor->GetFormID() == newest->form && (copies == 1 || (newest->unique != 0 && a_copy.unique == newest->unique));
				});
				if (newcomer == worn.end() || std::ranges::find(off, *newcomer) != off.end() || std::ranges::find(kept, *newcomer) != kept.end()) {
					continue;
				}
				const auto parts = newcomer->armor->GetSlotMask().underlying();
				for (const auto& other : worn) {
					if (!(other == *newcomer) && (other.armor->GetSlotMask().underlying() & parts) != 0 && std::ranges::find(kept, other) == kept.end() &&
						std::ranges::find(off, other) == off.end()) {
						off.push_back(other);
					}
				}
				kept.push_back(*newcomer);
			}
			return off;
		}

		bool ItemMenuOpen()
		{
			auto* ui = RE::UI::GetSingleton();
			return ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) || ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) ||
				ui->IsMenuOpen(RE::BarterMenu::MENU_NAME) || ui->IsMenuOpen(RE::GiftMenu::MENU_NAME);
		}

		// In the world the clashing copies go off at once, each with its own data. Taking one off from here behind
		// an open item menu left its list on freed item data and the game went down on the next update of the list,
		// so an open inventory takes them off through its own select. A twin of a kept piece can't be told apart in
		// the list, it waits for the menus to close like everything in other item menus
		void ResolveClashes(RE::PlayerCharacter& a_player)
		{
			const auto off = Clashing(a_player);
			if (!ItemMenuOpen()) {
				for (const auto& copy : off) {
					{
						IR_BRACKET("unequip {:08X} {} from the player, list {}", copy.armor->GetFormID(), copy.armor->GetName(), static_cast<const void*>(copy.data));
						RE::ActorEquipManager::GetSingleton()->UnequipObject(&a_player, copy.armor, copy.data, 1, nullptr, true, false, false);
					}
					logger::info("{} goes off, a new piece took its slot while it was hidden", copy.armor->GetName());
				}
				const std::scoped_lock hold{ putOnLock };
				putOn.clear();
				return;
			}
			const auto menu = RE::UI::GetSingleton()->GetMenu(RE::InventoryMenu::MENU_NAME);
			if (off.empty() || !menu || !menu->uiMovie) {
				return;
			}
			const auto worn = WornCopies(a_player);
			RE::GFxValue forms;
			menu->uiMovie->CreateArray(&forms);
			for (const auto& copy : off) {
				const auto copies = std::ranges::count_if(worn, [&](const WornCopy& a_copy) { return a_copy.armor == copy.armor; });
				if (copies == 1) {
					forms.PushBack(RE::GFxValue(static_cast<double>(copy.armor->GetFormID())));
					logger::info("{} goes off through the inventory, a new piece took its slot while it was hidden", copy.armor->GetName());
				}
			}
			if (forms.GetArraySize() > 0) {
				IR_SCRIPT("_global.Equipment.pushOff");
				menu->uiMovie->Invoke("_global.Equipment.pushOff", nullptr, &forms, 1);
			}
		}

		// A newer piece a follower's AI put on keeps its slots, every other worn copy on them goes: the game looked for
		// the clash among the pieces on the body, a hidden one isn't there. With the item menus closed only, their rows
		// hold the item data
		void ResolveFollowerClashes(RE::Actor& a_actor, const std::vector<RE::FormID>& a_newer)
		{
			auto* changes = a_actor.GetInventoryChanges();
			if (!changes || !changes->entryList) {
				return;
			}
			for (const auto newest : a_newer) {
				auto* kept = RE::TESForm::LookupByID<RE::TESObjectARMO>(newest);
				const auto worn = [&](RE::TESObjectARMO* a_armor) {
					return std::ranges::any_of(*changes->entryList, [&](RE::InventoryEntryData* a_entry) { return a_entry && a_entry->object == a_armor && a_entry->IsWorn(); });
				};
				if (!kept || !worn(kept)) {
					continue;
				}
				const auto parts = kept->GetSlotMask().underlying();
				std::vector<std::pair<RE::TESObjectARMO*, RE::ExtraDataList*>> off;
				for (auto* entry : *changes->entryList) {
					auto* armor = entry && entry->object ? entry->object->As<RE::TESObjectARMO>() : nullptr;
					if (!armor || armor == kept || !entry->extraLists || (armor->GetSlotMask().underlying() & parts) == 0 ||
						(armor->formFlags & RE::TESObjectARMO::RecordFlags::kNonPlayable) != 0) {
						continue;
					}
					for (auto* data : *entry->extraLists) {
						if (data && data->HasType(RE::ExtraDataType::kWorn)) {
							off.emplace_back(armor, data);
						}
					}
				}
				for (const auto& [armor, data] : off) {
					{
						IR_BRACKET("unequip {:08X} {} from {:08X}, list {}", armor->GetFormID(), armor->GetName(), a_actor.GetFormID(), static_cast<const void*>(data));
						RE::ActorEquipManager::GetSingleton()->UnequipObject(&a_actor, armor, data, 1, nullptr, true, false, false);
					}
					logger::info("{} goes off {}, {} took its slot while it was hidden", armor->GetName(), a_actor.GetName(), kept->GetName());
				}
			}
		}

		// A follower in the world, each frame on the main thread while something of theirs is hidden or owed: clashes
		// and a body owed are dealt with once the item menus are closed (the trade builds the one it shows itself), the
		// hair stays under a hidden helmet
		void FollowerFrame(RE::Actor& a_actor)
		{
			const auto id = a_actor.GetFormID();
			std::vector<RE::FormID> newer;
			bool rebuild = false;
			if (followerOwed.load() && !ItemMenuOpen()) {
				const std::scoped_lock hold{ followerOwedLock };
				for (const auto& put : followerPutOn) {
					if (put.actor == id) {
						newer.push_back(put.armor);
					}
				}
				std::erase_if(followerPutOn, [&](const PutOnBy& a_put) { return a_put.actor == id; });
				rebuild = std::erase(followerRebuilds, id) > 0;
				followerOwed = !followerRebuilds.empty() || !followerPutOn.empty();
			}
			if (!newer.empty()) {
				ResolveFollowerClashes(a_actor, newer);
			}
			if (rebuild && Subject::Rebuildable(a_actor)) {
				IR_BRACKET("the body of {:08X} is built again for its hidden slots", id);
				if (auto* process = a_actor.GetActorRuntimeData().currentProcess) {
					process->Set3DUpdateFlag(RE::RESET_3D_FLAGS::kModel);
				}
				a_actor.Update3DModel();
			} else if (rebuild) {
				OweRebuild(id);
			}
			if (const auto kept = KeptOf(a_actor)) {
				ShowCovered(a_actor, FollowerCover(a_actor, *kept));
			}
		}

		void FollowersFrame()
		{
			if (!anyFollowers.load() && !followerOwed.load()) {
				return;
			}
			static const auto work = Profile::Work("followers with hidden slots");
			const Profile::Timer timer{ work };
			std::vector<RE::FormID> ids;
			{
				const std::scoped_lock hold{ followersLock };
				for (const auto& [id, kept] : followers) {
					ids.push_back(id);
				}
			}
			{
				const std::scoped_lock hold{ followerOwedLock };
				for (const auto id : followerRebuilds) {
					ids.push_back(id);
				}
				for (const auto& put : followerPutOn) {
					ids.push_back(put.actor);
				}
			}
			std::ranges::sort(ids);
			ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
			for (const auto id : ids) {
				auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
				if (actor && actor->Is3DLoaded()) {
					FollowerFrame(*actor);
				}
			}
		}

		// Jewelry mods like MARA draw a worn piece themselves: a copy of its model hangs in the body under a node
		// of their own (MARA's is CORE_Rig, each copy a "Scene Root"). Nothing in the copy names the piece, but it
		// has the shapes of the piece's model with the piece's textures on them: a gold ring and a silver one of
		// one model differ only by the textures their armor addons swap in. A copy is known by its shapes with
		// their textures, or by the shapes alone when some mod changed the textures since. It goes out of sight
		// with the piece or its slot hidden
		using Shapes = std::vector<std::string>;

		struct Look
		{
			// the shape names, and each shape name with its diffuse texture
			Shapes shapes;
			Shapes dressed;
		};

		struct Hanging
		{
			RE::FormID form;
			bool hidden;
			std::vector<Look> looks;
		};

		std::string Lower(const char* a_name)
		{
			std::string lower{ a_name ? a_name : "" };
			std::ranges::transform(lower, lower.begin(), [](unsigned char a_char) { return static_cast<char>(std::tolower(a_char)); });
			return lower;
		}

		void Sort(Shapes& a_shapes)
		{
			std::ranges::sort(a_shapes);
			a_shapes.erase(std::unique(a_shapes.begin(), a_shapes.end()), a_shapes.end());
		}

		// a texture path however the game spells it, with or without data and textures in front
		std::string TexturePath(const char* a_path)
		{
			auto path = Lower(a_path);
			std::ranges::replace(path, '/', '\\');
			for (const auto lead : { "data\\"sv, "textures\\"sv }) {
				if (path.starts_with(lead)) {
					path.erase(0, lead.size());
				}
			}
			return path;
		}

		std::string Diffuse(RE::BSGeometry& a_shape)
		{
			auto* lighting = netimmerse_cast<RE::BSLightingShaderProperty*>(a_shape.GetGeometryRuntimeData().shaderProperty.get());
			auto* material = lighting ? static_cast<RE::BSLightingShaderMaterialBase*>(lighting->material) : nullptr;
			return TexturePath(material && material->textureSet ? material->textureSet->GetTexturePath(RE::BSTextureSet::Texture::kDiffuse) : nullptr);
		}

		std::string Dressed(const std::string& a_shape, const std::string& a_texture)
		{
			return a_shape + "|" + a_texture;
		}

		using Textured = std::vector<std::pair<std::string, std::string>>;

		// the shapes of a model file with their own textures, from the game's cache of models, kept per file.
		// Main thread only
		const Textured& ShapesOf(const char* a_path)
		{
			static std::unordered_map<std::string, Textured> known;
			const auto key = Lower(a_path);
			if (const auto found = known.find(key); found != known.end()) {
				return found->second;
			}
			Textured shapes;
			RE::NiPointer<RE::NiNode> loaded;
			const RE::BSModelDB::DBTraits::ArgsType how;
			if (RE::BSModelDB::Demand(a_path, loaded, how) == RE::BSResource::ErrorCode::kNone && loaded) {
				Scene::Visit(*loaded, [&](RE::NiAVObject& a_part) {
					if (auto* shape = a_part.AsGeometry()) {
						shapes.emplace_back(Lower(a_part.name.c_str()), Diffuse(*shape));
					}
				});
			}
			return known.emplace(key, std::move(shapes)).first->second;
		}

		// a piece in one of its models with the textures its form swaps in, a model of the other sex falls back to
		// the male one as the game does
		std::optional<Look> LookOf(const RE::TESModelTextureSwap& a_model, const RE::TESModelTextureSwap& a_male)
		{
			const auto* model = &a_model;
			const char* path = model->GetModel();
			if (!path || !*path) {
				model = &a_male;
				path = model->GetModel();
			}
			if (!path || !*path) {
				return std::nullopt;
			}
			Look look;
			for (const auto& [shape, texture] : ShapesOf(path)) {
				auto worn = texture;
				for (std::uint32_t i = 0; i < model->numAlternateTextures; ++i) {
					const auto& swap = model->alternateTextures[i];
					if (swap.textureSet && Lower(swap.name3D.c_str()) == shape) {
						worn = TexturePath(swap.textureSet->GetTexturePath(RE::BSTextureSet::Texture::kDiffuse));
					}
				}
				look.shapes.push_back(shape);
				look.dressed.push_back(Dressed(shape, worn));
			}
			if (look.shapes.empty()) {
				return std::nullopt;
			}
			Sort(look.shapes);
			Sort(look.dressed);
			return look;
		}

		// The looks of a piece in its worn models for the player's race and sex. Those models are in memory while
		// the piece is worn, so nothing is read from the disk. Kept per piece until the race or the sex changes.
		// Main thread only
		const std::vector<Look>& LooksOf(RE::TESObjectARMO& a_armor, RE::TESRace* a_race, RE::SEX a_sex)
		{
			static std::unordered_map<RE::FormID, std::vector<Look>> known;
			static std::pair<RE::TESRace*, RE::SEX> knownFor{ nullptr, RE::SEX::kNone };
			if (knownFor != std::pair{ a_race, a_sex }) {
				known.clear();
				knownFor = { a_race, a_sex };
			}
			if (const auto found = known.find(a_armor.GetFormID()); found != known.end()) {
				return found->second;
			}
			std::vector<Look> looks;
			for (const auto* addon : a_armor.armorAddons) {
				if (addon && a_race && addon->IsValidRace(a_race)) {
					if (auto look = LookOf(addon->bipedModels[a_sex], addon->bipedModels[RE::SEX::kMale])) {
						looks.push_back(std::move(*look));
					}
				}
			}
			return known.emplace(a_armor.GetFormID(), std::move(looks)).first->second;
		}

		std::vector<Hanging> WornModels(RE::PlayerCharacter& a_player)
		{
			std::vector<Hanging> worn;
			auto* race = a_player.GetRace();
			const auto* base = a_player.GetActorBase();
			const auto sex = base && base->GetSex() == RE::SEX::kFemale ? RE::SEX::kFemale : RE::SEX::kMale;
			auto* changes = a_player.GetInventoryChanges();
			if (!changes || !changes->entryList) {
				return worn;
			}
			for (auto* entry : *changes->entryList) {
				auto* armor = entry && entry->object ? entry->object->As<RE::TESObjectARMO>() : nullptr;
				if (!armor || !entry->IsWorn()) {
					continue;
				}
				if (const auto& looks = LooksOf(*armor, race, sex); !looks.empty()) {
					worn.push_back({ armor->GetFormID(), Skipped(armor), looks });
				}
			}
			return worn;
		}

		// the copies this file switched off, main thread only and never read through, only compared
		std::vector<const RE::NiAVObject*> culledHere;
		// pieces seen hanging once, the panel gives them an eye. Another mod hangs a new piece a few frames after
		// the list of the menu shows it, so a walk of the body right then could miss it
		std::mutex seenLock;
		std::vector<RE::FormID> seen;

		void Seen(RE::FormID a_form)
		{
			const std::scoped_lock hold{ seenLock };
			if (std::ranges::find(seen, a_form) == seen.end()) {
				seen.push_back(a_form);
			}
		}
		// another mod hangs its copies a frame or a few after an equip or a new body, so a pass runs that long
		constexpr std::int32_t kHungFrames = 30;
		std::atomic<std::int32_t> hungFrames{ 0 };

		// The worn piece a node is a copy of: by its shapes with their textures, else by its shapes alone. Pieces
		// that share that look can't be told apart, their copy stays hidden only while all of them are. The
		// textures are read only for a node whose shape names are those of a worn piece
		std::optional<Hanging> CopyOf(RE::NiAVObject& a_node, Shapes a_shapes, const std::vector<Hanging>& a_worn)
		{
			Sort(a_shapes);
			const bool known = std::ranges::any_of(a_worn, [&](const Hanging& a_piece) {
				return std::ranges::any_of(a_piece.looks, [&](const Look& a_look) { return a_look.shapes == a_shapes; });
			});
			if (!known) {
				return std::nullopt;
			}
			Shapes dressed;
			Scene::Visit(a_node, [&](RE::NiAVObject& a_part) {
				if (auto* shape = a_part.AsGeometry()) {
					dressed.push_back(Dressed(Lower(a_part.name.c_str()), Diffuse(*shape)));
				}
			});
			Sort(dressed);
			for (const bool byTexture : { true, false }) {
				std::optional<Hanging> found;
				for (const auto& piece : a_worn) {
					const bool same = std::ranges::any_of(piece.looks, [&](const Look& a_look) {
						return byTexture ? a_look.dressed == dressed : a_look.shapes == a_shapes;
					});
					if (!same) {
						continue;
					}
					Seen(piece.form);
					if (!found) {
						found = piece;
					} else {
						found->hidden = found->hidden && piece.hidden;
					}
				}
				if (found) {
					return found;
				}
			}
			return std::nullopt;
		}

		using Found = std::function<void(RE::NiAVObject&, const Hanging&)>;

		// Gathers the shapes under a node from the bottom up. The lowest node whose shapes match a piece is its
		// copy, a node above it holds other things too or only that copy. Tells whether a copy was found below
		bool FindCopies(RE::NiAVObject& a_node, const std::vector<Hanging>& a_worn, Shapes& a_shapes, const Found& a_found)
		{
			if (a_node.AsGeometry()) {
				a_shapes.push_back(Lower(a_node.name.c_str()));
				return false;
			}
			auto* node = a_node.AsNode();
			if (!node) {
				return false;
			}
			Shapes shapes;
			bool below = false;
			for (const auto& child : node->GetChildren()) {
				if (child) {
					below = FindCopies(*child, a_worn, shapes, a_found) || below;
				}
			}
			if (!below && !shapes.empty()) {
				if (const auto piece = CopyOf(a_node, shapes, a_worn)) {
					a_found(a_node, *piece);
					below = true;
				}
			}
			a_shapes.insert(a_shapes.end(), shapes.begin(), shapes.end());
			return below;
		}

		// calls a_found for every copy hanging in a_root with the worn piece it copies
		void ForEachHanging(RE::NiAVObject& a_root, const std::vector<Hanging>& a_worn, const Found& a_found)
		{
			if (a_worn.empty()) {
				return;
			}
			Shapes shapes;
			FindCopies(a_root, a_worn, shapes, a_found);
		}

		void HideHanging(RE::PlayerCharacter& a_player)
		{
			const auto worn = WornModels(a_player);
			std::vector<const RE::NiAVObject*> culled;
			for (const bool firstPerson : { false, true }) {
				auto* root = a_player.Get3D(firstPerson);
				if (!root) {
					continue;
				}
				ForEachHanging(*root, worn, [&](RE::NiAVObject& a_node, const Hanging& a_piece) {
					Seen(a_piece.form);
					const bool ours = std::ranges::find(culledHere, &a_node) != culledHere.end();
					if (a_piece.hidden && (ours || !a_node.GetAppCulled())) {
						if (!ours) {
							logger::info("{} of {:08X} hangs on the body, out of sight now", a_node.name.c_str(), a_piece.form);
						}
						a_node.SetAppCulled(true);
						culled.push_back(&a_node);
					} else if (!a_piece.hidden && ours) {
						logger::info("{} of {:08X} hangs on the body, in sight again", a_node.name.c_str(), a_piece.form);
						a_node.SetAppCulled(false);
					}
				});
			}
			culledHere = std::move(culled);
		}

		void StepHanging(RE::PlayerCharacter& a_player)
		{
			if (hungFrames.load() <= 0) {
				return;
			}
			--hungFrames;
			static const auto work = Profile::Work("hanging jewelry");
			const Profile::Timer timer{ work };
			HideHanging(a_player);
		}

		std::atomic<bool> clashesOwed{ false };

		// A request waits while the body isn't there, a load or a cell change builds it a moment later.
		// The task of the menus resolves clashes in an open inventory once per equip, the player's own update
		// only once the item menus are closed, and it builds the body while the world runs
		void RunOwed(RE::PlayerCharacter& a_player, bool a_fromMenus)
		{
			if (!a_player.Get3D(false)) {
				return;
			}
			if (clashesOwed.load() && (a_fromMenus || !ItemMenuOpen())) {
				ResolveClashes(a_player);
				// what is left waits for the item menus to close
				clashesOwed = ItemMenuOpen();
			}
			if (a_fromMenus && !RE::UI::GetSingleton()->GameIsPaused()) {
				return;
			}
			if (!rebuildOwed.exchange(false)) {
				return;
			}
			static const auto work = Profile::Work("body rebuilt for hidden slots");
			const Profile::Timer timer{ work };
			IR_BRACKET("the player's body is built again for the hidden slots, from {}", a_fromMenus ? "a menu" : "the world");
			if (auto* process = a_player.GetActorRuntimeData().currentProcess) {
				process->Set3DUpdateFlag(RE::RESET_3D_FLAGS::kModel);
			}
			a_player.Update3DModel();
			++wornChanges;
			ShowCovered(a_player);
		}

		// the player's own update each frame, on the main thread. The look costs a flag while nothing is hidden.
		// It's the plugin's frame of the world too, the quests read what they point at here after a load
		struct PlayerUpdate
		{
			static void thunk(RE::PlayerCharacter* a_player, float a_delta)
			{
#if IR_DEBUG_EDITION
				Debug::WorldFrame();
#endif
				original(a_player, a_delta);
				if (AnythingHidden() || rebuildOwed.load() || clashesOwed.load()) {
					static const auto work = Profile::Work("world frame with hidden slots");
					const Profile::Timer timer{ work };
					RunOwed(*a_player, false);
					ShowCovered(*a_player);
				}
				FollowersFrame();
				Keep::Frame();
				StepHanging(*a_player);
				Quests::Frame();
			}

			static inline REL::Relocation<decltype(thunk)> original;
		};

		// asked from an event or a hook, taken by the next frame of an item menu
		std::atomic<bool> runInMenu{ false };

		// Requests come from any thread. They run in the player's own update, or on the thread of the menus while
		// a menu holds the world still, so the doll in an open inventory follows at once. An event or a hook only
		// marks it, SKSE runs its tasks under the lock of its queue and the game may hold a lock of its own around
		// the event, a task queued from there could wait on a task that waits on that lock
		void RunSoon()
		{
			runInMenu = true;
		}

		// hiding, showing and a piece put into a hidden slot have the worn pieces go on again
		void Rebuild()
		{
			++wornChanges;
			rebuildOwed = true;
			hungFrames = kHungFrames;
			RunSoon();
		}

		// While something is hidden: a piece put into a hidden slot goes onto the body the usual way, so the worn
		// pieces go on again without it. Any other piece of armor only gets the look for hidden pieces it pushes off
		struct Equips : RE::BSTEventSink<RE::TESEquipEvent>
		{
			RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
			{
				if (!a_event) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (a_event->actor.get() != RE::PlayerCharacter::GetSingleton()) {
					Follower(*a_event);
					return RE::BSEventNotifyControl::kContinue;
				}
				++wornChanges;
				hungFrames = kHungFrames;
				auto* object = RE::TESForm::LookupByID<RE::TESBoundObject>(a_event->baseObject);
				// pushes are kept while nothing is hidden too, the helmet usually goes on long before it's hidden
				auto* armor = object ? object->As<RE::TESObjectARMO>() : nullptr;
				if (armor && (armor->formFlags & RE::TESObjectARMO::RecordFlags::kNonPlayable) == 0) {
					if (const auto by = Remember(*a_event, *armor); by != 0) {
						++wornChanges;
						if (Skipped(RE::TESForm::LookupByID<RE::TESBoundObject>(by))) {
							Rebuild();
						}
					}
				}
				if (!a_event->equipped || !AnythingHidden() || !object) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (object->Is(RE::FormType::Armor)) {
					{
						const std::scoped_lock hold{ putOnLock };
						putOn.push_back({ a_event->baseObject, a_event->uniqueID });
					}
					clashesOwed = true;
				}
				if (Skipped(object) || CoverSkipped(*RE::PlayerCharacter::GetSingleton(), object)) {
					Rebuild();
				} else if (clashesOwed.load()) {
					RunSoon();
				}
				return RE::BSEventNotifyControl::kContinue;
			}

			// A follower with something hidden: a piece put into a hidden slot went onto the body the usual way, the
			// body is built again without it. Any piece of armor put on is looked at for a clash
			static void Follower(const RE::TESEquipEvent& a_event)
			{
				auto* actor = a_event.actor ? a_event.actor->As<RE::Actor>() : nullptr;
				const auto kept = actor && a_event.equipped ? KeptOf(*actor) : std::nullopt;
				auto* object = kept ? RE::TESForm::LookupByID<RE::TESBoundObject>(a_event.baseObject) : nullptr;
				auto* armor = object ? object->As<RE::TESObjectARMO>() : nullptr;
				if (!armor || (armor->formFlags & RE::TESObjectARMO::RecordFlags::kNonPlayable) != 0) {
					return;
				}
				{
					const std::scoped_lock hold{ followerOwedLock };
					if (followerPutOn.size() < kMostFollowers) {
						followerPutOn.push_back({ actor->GetFormID(), armor->GetFormID() });
					}
					followerOwed = true;
				}
				if (SkippedBy(armor, kept->mask, [&](RE::FormID a_form) { return kept->Hides(a_form); })) {
					OweRebuild(actor->GetFormID());
				}
			}
		};

		// the game built the player's body anew, a load or a new cell, and jewelry mods hang their copies on it again
		struct Loads : RE::BSTEventSink<RE::TESObjectLoadedEvent>
		{
			RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override
			{
				if (a_event && a_event->loaded && a_event->formID == RE::PlayerCharacter::GetSingleton()->GetFormID()) {
					hungFrames = kHungFrames;
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> table{ RE::VTABLE___InitWornVisitor[0] };
		Visit::original = table.write_vfunc(kVisit, Visit::thunk);

		constexpr std::size_t kUpdate = 0xAD;
		REL::Relocation<std::uintptr_t> player{ RE::VTABLE_PlayerCharacter[0] };
		PlayerUpdate::original = player.write_vfunc(kUpdate, PlayerUpdate::thunk);
	}

	void Listen()
	{
		static Equips equips;
		static Loads loads;
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESEquipEvent>(&equips);
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESObjectLoadedEvent>(&loads);

		constexpr auto kGentleman = "TheNewGentleman.esp"sv;
		auto* files = RE::TESDataHandler::GetSingleton();
		tng.cover = files->LookupForm<RE::TESObjectARMO>(0xAFF, kGentleman);
		tng.covering = files->LookupForm<RE::BGSKeyword>(0xFFD, kGentleman);
		tng.revealingOnlyWomen = files->LookupForm<RE::BGSKeyword>(0xFFB, kGentleman);
		tng.revealingOnlyMen = files->LookupForm<RE::BGSKeyword>(0xFFC, kGentleman);
		if (tng.cover) {
			logger::info("TNG is here, its genital cover {:08X} goes off with hidden pieces when nothing shown covers", tng.cover->GetFormID());
		}
	}

	std::uint32_t Mask()
	{
		return mask.load();
	}

	bool CanHide(std::int32_t a_slot)
	{
		return a_slot >= 0 && a_slot < kCount && a_slot != kRight && a_slot != kLeft;
	}

	bool Set(std::int32_t a_slot, bool a_hidden)
	{
		if (!CanHide(a_slot)) {
			return false;
		}
		const std::uint32_t bit = 1U << a_slot;
		const auto before = mask.load();
		const auto after = a_hidden ? (before | bit) : (before & ~bit);
		if (after == before) {
			return true;
		}
		mask.store(after);
		logger::info("slot {} is {} on the player now, hidden slots {:X}", a_slot, a_hidden ? "hidden" : "shown", after);
		Rebuild();
		return true;
	}

	std::vector<RE::FormID> Pieces()
	{
		const std::scoped_lock hold{ piecesLock };
		return pieces;
	}

	void SetPiece(RE::FormID a_form, bool a_hidden)
	{
		auto now = Pieces();
		const bool there = std::ranges::find(now, a_form) != now.end();
		if (there == a_hidden || a_form == 0) {
			return;
		}
		if (a_hidden) {
			now.push_back(a_form);
		} else {
			std::erase(now, a_form);
		}
		StorePieces(std::move(now));
		const auto* form = RE::TESForm::LookupByID(a_form);
		logger::info("piece {:08X} {} is {} on the player now", a_form, form ? form->GetName() : "", a_hidden ? "hidden" : "shown");
		Rebuild();
	}

	void ShowAll()
	{
		const bool pieced = anyPieces.load();
		StorePieces({});
		if (mask.exchange(0) != 0 || pieced) {
			logger::info("every hidden slot and piece is shown on the player again");
			Rebuild();
		}
		std::vector<RE::FormID> shown;
		{
			const std::scoped_lock hold{ followersLock };
			for (const auto& [id, kept] : followers) {
				shown.push_back(id);
			}
			followers.clear();
			anyFollowers = false;
		}
		for (const auto id : shown) {
			OweRebuild(id);
		}
		if (!shown.empty()) {
			logger::info("every hidden slot and piece is shown on {} followers again", shown.size());
		}
	}

	std::uint32_t MaskOf(RE::Actor& a_actor)
	{
		if (a_actor.IsPlayerRef()) {
			return Mask();
		}
		const auto kept = KeptOf(a_actor);
		return kept ? kept->mask : 0;
	}

	std::vector<RE::FormID> PiecesOf(RE::Actor& a_actor)
	{
		if (a_actor.IsPlayerRef()) {
			return Pieces();
		}
		const auto kept = KeptOf(a_actor);
		return kept ? kept->pieces : std::vector<RE::FormID>{};
	}

	namespace
	{
		// changes what a follower keeps hidden, false when a new follower would pass the most kept
		bool ChangeKept(RE::Actor& a_actor, const std::function<void(Kept&)>& a_change)
		{
			const std::scoped_lock hold{ followersLock };
			const auto id = a_actor.GetFormID();
			auto found = followers.find(id);
			if (found == followers.end()) {
				if (followers.size() >= kMostFollowers) {
					logger::warn("{} hides nothing, {} followers keep hidden slots already", a_actor.GetName(), kMostFollowers);
					return false;
				}
				found = followers.emplace(id, Kept{}).first;
			}
			a_change(found->second);
			if (found->second.Empty()) {
				followers.erase(found);
			}
			anyFollowers = !followers.empty();
			return true;
		}
	}

#if IR_DOVAKARN
	namespace
	{
		// Dovakarn: hiding worn gear is refused in its build whatever the panel's movie asks (the panel's SetHidden and
		// SetHiddenPiece). A hidden piece vanished in the player's own game only, every other player still drew it from
		// what the server says they wear. Showing a piece again stays, it only ever undoes a hide
		bool RefusedHide(bool a_hidden)
		{
			if (a_hidden) {
				static std::atomic<bool> told{ false };
				if (!told.exchange(true)) {
					logger::info("Dovakarn's build: hiding worn gear is refused");
				}
			}
			return a_hidden;
		}
	}
#endif

	bool SetOn(RE::Actor& a_actor, std::int32_t a_slot, bool a_hidden)
	{
#if IR_DOVAKARN
		if (RefusedHide(a_hidden)) {
			return false;
		}
#endif
		if (a_actor.IsPlayerRef()) {
			return Set(a_slot, a_hidden);
		}
		if (!CanHide(a_slot)) {
			return false;
		}
		const std::uint32_t bit = 1U << a_slot;
		std::uint32_t after = 0;
		const bool kept = ChangeKept(a_actor, [&](Kept& a_kept) {
			a_kept.mask = a_hidden ? (a_kept.mask | bit) : (a_kept.mask & ~bit);
			after = a_kept.mask;
		});
		if (kept) {
			logger::info("slot {} is {} on {} now, hidden slots {:X}", a_slot, a_hidden ? "hidden" : "shown", a_actor.GetName(), after);
			OweRebuild(a_actor.GetFormID());
		}
		return kept;
	}

	void SetPieceOn(RE::Actor& a_actor, RE::FormID a_form, bool a_hidden)
	{
#if IR_DOVAKARN
		if (RefusedHide(a_hidden)) {
			return;
		}
#endif
		if (a_actor.IsPlayerRef()) {
			SetPiece(a_form, a_hidden);
			return;
		}
		if (a_form == 0) {
			return;
		}
		const bool kept = ChangeKept(a_actor, [&](Kept& a_kept) {
			std::erase(a_kept.pieces, a_form);
			if (a_hidden) {
				a_kept.pieces.push_back(a_form);
			}
		});
		if (kept) {
			const auto* form = RE::TESForm::LookupByID(a_form);
			logger::info("piece {:08X} {} is {} on {} now", a_form, form ? form->GetName() : "", a_hidden ? "hidden" : "shown", a_actor.GetName());
			OweRebuild(a_actor.GetFormID());
		}
	}

	bool Hides(RE::Actor& a_actor, RE::TESBoundObject* a_object)
	{
		return a_actor.IsPlayerRef() ? Skipped(a_object) : FollowerSkips(a_actor, a_object);
	}

	void BodyBuilt(RE::Actor& a_actor)
	{
		if (const auto kept = KeptOf(a_actor)) {
			ShowCovered(a_actor, FollowerCover(a_actor, *kept));
		}
	}

	void Save(SKSE::SerializationInterface* a_save)
	{
		if (!a_save->OpenRecord(kRecord, kVersion)) {
			logger::error("the hidden slots could not be saved");
			return;
		}
		const auto hidden = Pieces();
		a_save->WriteRecordData(mask.load());
		a_save->WriteRecordData(static_cast<std::uint32_t>(hidden.size()));
		for (const auto form : hidden) {
			a_save->WriteRecordData(form);
		}
		const auto pushed = Pushes();
		a_save->WriteRecordData(static_cast<std::uint32_t>(pushed.size()));
		for (const auto& push : pushed) {
			a_save->WriteRecordData(push.by);
			a_save->WriteRecordData(push.off);
		}
		const std::scoped_lock hold{ followersLock };
		if (followers.empty()) {
			return;
		}
		if (!a_save->OpenRecord(kFollowersRecord, kFollowersVersion)) {
			logger::error("the hidden slots of followers could not be saved");
			return;
		}
		a_save->WriteRecordData(static_cast<std::uint32_t>(followers.size()));
		for (const auto& [id, kept] : followers) {
			a_save->WriteRecordData(id);
			a_save->WriteRecordData(kept.mask);
			a_save->WriteRecordData(static_cast<std::uint32_t>(kept.pieces.size()));
			for (const auto form : kept.pieces) {
				a_save->WriteRecordData(form);
			}
		}
	}

	void ReadFollowers(SKSE::SerializationInterface* a_save, std::uint32_t a_version)
	{
		std::uint32_t count = 0;
		if (a_version != kFollowersVersion || !a_save->ReadRecordData(count)) {
			return;
		}
		std::unordered_map<RE::FormID, Kept> read;
		for (std::uint32_t at = 0; at < count && at < kMostFollowers; ++at) {
			RE::FormID saved = 0;
			RE::FormID actor = 0;
			Kept kept;
			std::uint32_t pieces = 0;
			if (!a_save->ReadRecordData(saved) || !a_save->ReadRecordData(kept.mask) || !a_save->ReadRecordData(pieces)) {
				break;
			}
			// every piece is read so the next follower starts in its place, a mod taken out lets its pieces go
			for (std::uint32_t piece = 0; piece < pieces; ++piece) {
				RE::FormID form = 0;
				RE::FormID now = 0;
				// a record cut short ends the reading, the count of a broken one could be anything
				if (!a_save->ReadRecordData(form)) {
					break;
				}
				if (a_save->ResolveFormID(form, now)) {
					kept.pieces.push_back(now);
				}
			}
			kept.mask &= ~((1U << kRight) | (1U << kLeft));
			if (a_save->ResolveFormID(saved, actor) && !kept.Empty()) {
				read.emplace(actor, std::move(kept));
			}
		}
#if IR_DOVAKARN
		// Dovakarn: nothing hidden comes back from a save either (SetOn)
		read.clear();
#endif
		const std::scoped_lock hold{ followersLock };
		followers = std::move(read);
		anyFollowers = !followers.empty();
		logger::info("{} followers keep hidden slots or pieces, read from the save", followers.size());
	}

	void Read(SKSE::SerializationInterface* a_save, std::uint32_t a_version)
	{
		std::uint32_t read = 0;
		if (a_version < 1 || a_version > kVersion || !a_save->ReadRecordData(read)) {
			return;
		}
		// a hand hidden by a test build before hands were left out shows again
		read &= ~((1U << kRight) | (1U << kLeft));
		mask.store(read);
		std::vector<RE::FormID> found;
		std::uint32_t count = 0;
		if (a_version >= 2 && a_save->ReadRecordData(count)) {
			for (std::uint32_t at = 0; at < count; ++at) {
				RE::FormID saved = 0;
				RE::FormID now = 0;
				if (!a_save->ReadRecordData(saved)) {
					break;
				}
				// a piece of a mod taken out of the load order is let go
				if (a_save->ResolveFormID(saved, now)) {
					found.push_back(now);
				}
			}
		}
#if IR_DOVAKARN
		// Dovakarn: nothing hidden comes back from a save either (SetOn)
		read = 0;
		mask.store(0);
		found.clear();
#endif
		StorePieces(std::move(found));
		std::vector<Push> kept;
		if (a_version >= 3 && a_save->ReadRecordData(count)) {
			for (std::uint32_t at = 0; at < count; ++at) {
				Push saved{};
				Push now{};
				if (!a_save->ReadRecordData(saved.by) || !a_save->ReadRecordData(saved.off)) {
					break;
				}
				if (a_save->ResolveFormID(saved.by, now.by) && a_save->ResolveFormID(saved.off, now.off)) {
					kept.push_back(now);
				}
			}
		}
		{
			const std::scoped_lock hold{ pushesLock };
			pushes = std::move(kept);
			lastOn = {};
			lastOff = {};
		}
		++wornChanges;
		logger::info("hidden slots {:X}, {} single pieces and {} pushed off wigs are read from the save", read, Pieces().size(), Pushes().size());
	}

	void Refresh()
	{
		hungFrames = kHungFrames;
		if (AnythingHidden()) {
			Rebuild();
		}
	}

	void MenuFrame()
	{
		if (runInMenu.exchange(false)) {
			SKSE::GetTaskInterface()->AddUITask([] {
				if (auto* player = RE::PlayerCharacter::GetSingleton()) {
					RunOwed(*player, true);
				}
			});
		}
		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			StepHanging(*player);
		}
	}

	std::vector<RE::FormID> Hung()
	{
		const std::scoped_lock hold{ seenLock };
		return seen;
	}

	void DumpHanging()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		const auto worn = WornModels(*player);
		for (const auto& piece : worn) {
			const auto* form = RE::TESForm::LookupByID(piece.form);
			std::string looks;
			for (const auto& look : piece.looks) {
				looks += "[";
				for (const auto& shape : look.dressed) {
					looks += shape + " ";
				}
				looks += "] ";
			}
			logger::info("worn {:08X} {}{}: {}", piece.form, form ? form->GetName() : "", piece.hidden ? " (hidden)" : "", looks);
		}
		// every copy found with three parents up, and every node under a node of another mod's rig
		const auto chain = [](RE::NiAVObject& a_node) {
			std::string up;
			auto* parent = a_node.parent;
			for (int i = 0; i < 3 && parent; ++i, parent = parent->parent) {
				up += std::string(" < ") + parent->name.c_str();
			}
			return up;
		};
		for (const bool firstPerson : { false, true }) {
			auto* root = player->Get3D(firstPerson);
			if (!root) {
				continue;
			}
			const auto* where = firstPerson ? "first person" : "body";
			ForEachHanging(*root, worn, [&](RE::NiAVObject& a_node, const Hanging& a_piece) {
				std::string textures;
				Scene::Visit(a_node, [&](RE::NiAVObject& a_part) {
					if (auto* shape = a_part.AsGeometry()) {
						textures += Dressed(Lower(a_part.name.c_str()), Diffuse(*shape)) + " ";
					}
				});
				logger::info("{}: copy of {:08X} '{}'{}, culled {}, {}", where, a_piece.form, a_node.name.c_str(), chain(a_node), a_node.GetAppCulled(), textures);
			});
			Scene::Visit(*root, [&](RE::NiAVObject& a_node) {
				if (a_node.parent && std::string_view(a_node.parent->name.c_str()).find("Rig") != std::string_view::npos) {
					logger::info("{}: under a rig {} '{}'{}", where, a_node.AsGeometry() ? "shape" : "node", a_node.name.c_str(), chain(a_node));
				}
			});
		}
	}

	void HideHanging(RE::NiAVObject& a_copy)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		ForEachHanging(a_copy, WornModels(*player), [](RE::NiAVObject& a_node, const Hanging& a_piece) {
			Seen(a_piece.form);
			if (a_piece.hidden) {
				a_node.SetAppCulled(true);
			}
		});
	}

	void UncoverCopiedFace(RE::NiAVObject& a_face, RE::PlayerCharacter& a_player)
	{
		UncoverFace(&a_face, a_player, HiddenCover(a_player), false);
	}

	bool HeadUnderShownPiece(RE::PlayerCharacter& a_player)
	{
		constexpr std::uint32_t kHeadSlot = 1;
		return (CoversOf(a_player).shown & kHeadSlot) != 0;
	}

	void Forget()
	{
		mask.store(0);
		StorePieces({});
		{
			const std::scoped_lock hold{ followersLock };
			followers.clear();
			anyFollowers = false;
		}
		{
			const std::scoped_lock hold{ followerOwedLock };
			followerRebuilds.clear();
			followerPutOn.clear();
			followerOwed = false;
		}
		{
			const std::scoped_lock hold{ pushesLock };
			pushes.clear();
			lastOn = {};
			lastOff = {};
		}
		++wornChanges;
	}
}
