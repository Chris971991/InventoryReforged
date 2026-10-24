#include "PCH.h"

#include "Motion.h"

#include "PlayerView.h"
#include "Scene.h"
#include "Settings.h"
#include "Subject.h"

namespace Motion
{
	namespace
	{
		constexpr std::size_t kNotify = 1;
		constexpr std::string_view kBonePrefix = "NPC ";
		constexpr auto kOffsetStop = "OffsetStop";
		// the doll goes over into the move and back in this many seconds
		constexpr float kFadeIn = 0.25F;
		constexpr float kFadeOut = 0.35F;
		// A move is over once no bone of the player turned faster than about 20 degrees a second for a while.
		// Breathing turns far slower, a hand putting a helmet on far faster
		constexpr float kStillTurn = 0.35F;
		constexpr float kStillFor = 0.4F;
		// the least and the most a move gets in seconds, its first frames may be the graph blending in
		constexpr float kLeast = 0.8F;
		constexpr float kMost = 8.0F;
		// a longer frame is a hitch, the graph steps no further than this
		constexpr float kLongestStep = 0.05F;

		struct Bone
		{
			RE::NiAVObject* live;
			RE::NiAVObject* doll;
			RE::NiTransform rest;
			RE::NiQuaternion restTurn;
			RE::NiQuaternion liveTurn;
		};

		// events come on the threads of Papyrus, the rest lives on the game's own thread
		std::mutex heardLock;
		std::vector<std::string> heard;
		std::size_t begun = 0;
		std::atomic<bool> watching{ false };

		bool moving = false;
		bool ending = false;
		// the graph was stepped under a paused menu, the world then gets the events again
		bool stepped = false;
		std::vector<Bone> bones;
		std::size_t pairedWith = 0;
		RE::NiAVObject* pairedLive = nullptr;
		std::chrono::steady_clock::time_point lastFrame;
		float since = 0.0F;
		float stillFor = 0.0F;
		float weight = 0.0F;
		// what the log says of a move once it ends
		std::size_t steps = 0;
		float fastestOfMove = 0.0F;

		RE::NiQuaternion TurnOf(const RE::NiMatrix3& a_matrix)
		{
			const auto& m = a_matrix.entry;
			RE::NiQuaternion turn;
			const float trace = m[0][0] + m[1][1] + m[2][2];
			if (trace > 0.0F) {
				const float s = std::sqrt(trace + 1.0F) * 2.0F;
				turn.w = 0.25F * s;
				turn.x = (m[2][1] - m[1][2]) / s;
				turn.y = (m[0][2] - m[2][0]) / s;
				turn.z = (m[1][0] - m[0][1]) / s;
			} else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
				const float s = std::sqrt(1.0F + m[0][0] - m[1][1] - m[2][2]) * 2.0F;
				turn.w = (m[2][1] - m[1][2]) / s;
				turn.x = 0.25F * s;
				turn.y = (m[0][1] + m[1][0]) / s;
				turn.z = (m[0][2] + m[2][0]) / s;
			} else if (m[1][1] > m[2][2]) {
				const float s = std::sqrt(1.0F + m[1][1] - m[0][0] - m[2][2]) * 2.0F;
				turn.w = (m[0][2] - m[2][0]) / s;
				turn.x = (m[0][1] + m[1][0]) / s;
				turn.y = 0.25F * s;
				turn.z = (m[1][2] + m[2][1]) / s;
			} else {
				const float s = std::sqrt(1.0F + m[2][2] - m[0][0] - m[1][1]) * 2.0F;
				turn.w = (m[1][0] - m[0][1]) / s;
				turn.x = (m[0][2] + m[2][0]) / s;
				turn.y = (m[1][2] + m[2][1]) / s;
				turn.z = 0.25F * s;
			}
			return turn;
		}

		RE::NiMatrix3 MatrixOf(const RE::NiQuaternion& a_turn)
		{
			const float w = a_turn.w;
			const float x = a_turn.x;
			const float y = a_turn.y;
			const float z = a_turn.z;
			RE::NiMatrix3 matrix;
			matrix.entry[0][0] = 1.0F - 2.0F * (y * y + z * z);
			matrix.entry[0][1] = 2.0F * (x * y - z * w);
			matrix.entry[0][2] = 2.0F * (x * z + y * w);
			matrix.entry[1][0] = 2.0F * (x * y + z * w);
			matrix.entry[1][1] = 1.0F - 2.0F * (x * x + z * z);
			matrix.entry[1][2] = 2.0F * (y * z - x * w);
			matrix.entry[2][0] = 2.0F * (x * z - y * w);
			matrix.entry[2][1] = 2.0F * (y * z + x * w);
			matrix.entry[2][2] = 1.0F - 2.0F * (x * x + y * y);
			return matrix;
		}

		float Dot(const RE::NiQuaternion& a_one, const RE::NiQuaternion& a_other)
		{
			return a_one.w * a_other.w + a_one.x * a_other.x + a_one.y * a_other.y + a_one.z * a_other.z;
		}

		// the angle in radians between two turns
		float Between(const RE::NiQuaternion& a_one, const RE::NiQuaternion& a_other)
		{
			return 2.0F * std::acos(std::clamp(std::abs(Dot(a_one, a_other)), 0.0F, 1.0F));
		}

		RE::NiQuaternion Blend(const RE::NiQuaternion& a_from, RE::NiQuaternion a_to, float a_share)
		{
			float dot = Dot(a_from, a_to);
			if (dot < 0.0F) {
				a_to = { -a_to.w, -a_to.x, -a_to.y, -a_to.z };
				dot = -dot;
			}
			float from = 1.0F - a_share;
			float to = a_share;
			// nearly the same turn, a straight blend is as good and has no division by a tiny sine
			if (dot < 0.9995F) {
				const float angle = std::acos(dot);
				const float sine = std::sin(angle);
				from = std::sin(from * angle) / sine;
				to = std::sin(to * angle) / sine;
			}
			RE::NiQuaternion blend{ from * a_from.w + to * a_to.w, from * a_from.x + to * a_to.x, from * a_from.y + to * a_to.y, from * a_from.z + to * a_to.z };
			const float length = std::sqrt(Dot(blend, blend));
			if (length > 0.0F) {
				blend = { blend.w / length, blend.x / length, blend.y / length, blend.z / length };
			}
			return blend;
		}

		// the bones of the doll with the same bones of the player, the doll as it stands now is its rest
		void Pair(RE::NiAVObject& a_doll, RE::NiAVObject& a_live)
		{
			std::unordered_map<std::string_view, RE::NiAVObject*> live;
			Scene::Visit(a_live, [&](RE::NiAVObject& a_part) {
				const std::string_view name = a_part.name.c_str() ? a_part.name.c_str() : "";
				if (a_part.AsNode() && name.starts_with(kBonePrefix)) {
					live.emplace(name, &a_part);
				}
			});
			bones.clear();
			Scene::Visit(a_doll, [&](RE::NiAVObject& a_part) {
				const std::string_view name = a_part.name.c_str() ? a_part.name.c_str() : "";
				if (!a_part.AsNode() || !name.starts_with(kBonePrefix)) {
					return;
				}
				if (const auto found = live.find(name); found != live.end()) {
					bones.push_back({ found->second, &a_part, a_part.local, TurnOf(a_part.local.rotate), TurnOf(found->second->local.rotate) });
				}
			});
			pairedWith = PlayerView::Made();
			pairedLive = &a_live;
		}

		void Pose(RE::NiAVObject& a_doll, float a_weight)
		{
			for (const auto& bone : bones) {
				bone.doll->local.rotate = MatrixOf(Blend(bone.restTurn, bone.liveTurn, a_weight));
				bone.doll->local.translate = bone.rest.translate + (bone.live->local.translate - bone.rest.translate) * a_weight;
			}
			RE::NiUpdateData update;
			a_doll.Update(update);
		}

		void Settle()
		{
			if (moving) {
				logger::info("the doll's move ended after {:.1f} s, the graph stepped {} times under the menu, its fastest bone turned {:.0f} degrees a second", since, steps,
					fastestOfMove * 180.0F / std::numbers::pi_v<float>);
			}
			if (auto* doll = PlayerView::Doll(); doll && pairedWith == PlayerView::Made()) {
				Pose(*doll, 0.0F);
			}
			moving = false;
			ending = false;
			weight = 0.0F;
			bones.clear();
			pairedWith = 0;
			pairedLive = nullptr;
		}

		struct Notify
		{
			static bool thunk(RE::IAnimationGraphManagerHolder* a_holder, const RE::BSFixedString& a_event)
			{
				IR_HOOK("the player's animation event heard");
				const bool taken = original(a_holder, a_event);
				if (taken && watching.load() && a_event.c_str()) {
					const std::scoped_lock lock{ heardLock };
					heard.emplace_back(a_event.c_str());
				}
				return taken;
			}

			static inline REL::Relocation<decltype(thunk)> original;
		};
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> table{ RE::VTABLE_PlayerCharacter[3] };
		Notify::original = table.write_vfunc(kNotify, Notify::thunk);
	}

	void Frame()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* doll = PlayerView::Doll();
		// a follower's doll stands still, the moves heard are the player's
		if (!player || !doll || !Settings::Get().equipMotion || Subject::Other()) {
			watching = false;
			if (moving) {
				Settle();
			}
			return;
		}
		watching = true;
		const auto now = std::chrono::steady_clock::now();
		{
			const std::scoped_lock lock{ heardLock };
			if (heard.size() > begun) {
				logger::info("the doll takes over the move of the player, the animation graph heard \"{}\", the game {} (time {})", heard.back(),
					RE::UI::GetSingleton()->GameIsPaused() ? "stands still under the menu" : "runs under the menu", RE::Main::GetSingleton()->freezeTime ? "frozen" : "running");
				begun = heard.size();
				if (!moving) {
					moving = true;
					weight = 0.0F;
					lastFrame = now;
					steps = 0;
					fastestOfMove = 0.0F;
				}
				ending = false;
				since = 0.0F;
				stillFor = 0.0F;
			}
		}
		if (!moving) {
			return;
		}
		const float step = std::clamp(std::chrono::duration<float>(now - lastFrame).count(), 0.0F, kLongestStep);
		lastFrame = now;
		// under a paused menu nothing moves the graph, with Skyrim Souls RE and the like the game does
		if (RE::UI::GetSingleton()->GameIsPaused() && step > 0.0F) {
			IR_COUNT("the player's animation stepped under a paused menu");
			player->UpdateAnimation(step);
			stepped = true;
			++steps;
		}
		auto* live = player->Get3D(false);
		if (!live) {
			Settle();
			return;
		}
		if (pairedWith != PlayerView::Made() || pairedLive != live) {
			Pair(*doll, *live);
		}
		since += step;
		float fastest = 0.0F;
		for (auto& bone : bones) {
			const auto turn = TurnOf(bone.live->local.rotate);
			if (step > 0.0F) {
				fastest = std::max(fastest, Between(turn, bone.liveTurn) / step);
			}
			bone.liveTurn = turn;
		}
		fastestOfMove = std::max(fastestOfMove, fastest);
		stillFor = fastest < kStillTurn ? stillFor + step : 0.0F;
		if (!ending && (since > kMost || (since > kLeast && stillFor > kStillFor))) {
			ending = true;
		}
		weight = ending ? weight - step / kFadeOut : std::min(1.0F, weight + step / kFadeIn);
		if (ending && weight <= 0.0F) {
			Settle();
			return;
		}
		Pose(*doll, weight);
	}

	void MenuClosed()
	{
		watching = false;
		SKSE::GetTaskInterface()->AddTask([] {
			std::vector<std::string> again;
			{
				const std::scoped_lock lock{ heardLock };
				if (stepped) {
					again = heard;
				}
				heard.clear();
				begun = 0;
			}
			Settle();
			stepped = false;
			// The menu took the moves the world was to play once it closed, it plays them now. A move played out
			// holds its last frame until its mod stops it, and the same event again doesn't start it anew. FNIS
			// offset moves stop on OffsetStop, so the graph gets that first
			auto* player = RE::PlayerCharacter::GetSingleton();
			IR_BRACKET("the world gets {} animation events of the player again", again.size());
			if (player && !again.empty()) {
				player->NotifyAnimationGraph(RE::BSFixedString(kOffsetStop));
			}
			for (const auto& event : again) {
				if (player) {
					player->NotifyAnimationGraph(RE::BSFixedString(event));
				}
			}
			if (!again.empty()) {
				logger::info("the world gets {} animation events of the player again, the doll played them under the menu", again.size());
			}
		});
	}
}
