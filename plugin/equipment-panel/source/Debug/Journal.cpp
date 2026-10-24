#include "PCH.h"

#include "Debug/Journal.h"

#include <cstring>

#include "Debug/Log.h"
#include "Debug/Script.h"
#include "Subject.h"

namespace Debug::Journal
{
	namespace
	{
		constexpr RE::FormID kPlayer = 0x14;

		bool Watched(RE::FormID a_actor)
		{
			return a_actor != 0 && (a_actor == kPlayer || a_actor == Subject::ShownForm());
		}

		class Menus : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event) {
					MenuChanged(a_event->menuName, a_event->opening);
					Said("menu", fmt::format("menu {} {}", a_event->menuName.c_str() ? a_event->menuName.c_str() : "?", a_event->opening ? "opens" : "closes"));
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// only form ids here, the events come on any thread and a name may want a lock of the game
		class Equips : public RE::BSTEventSink<RE::TESEquipEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
			{
				const auto actor = a_event && a_event->actor ? a_event->actor->GetFormID() : 0;
				if (Watched(actor)) {
					Said("equip", fmt::format("equip event: {:08X} {} {:08X}, unique id {}", actor, a_event->equipped ? "put on" : "took off", a_event->baseObject, a_event->uniqueID));
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class Moves : public RE::BSTEventSink<RE::TESContainerChangedEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* a_event, RE::BSTEventSource<RE::TESContainerChangedEvent>*) override
			{
				if (a_event && (Watched(a_event->oldContainer) || Watched(a_event->newContainer))) {
					Said("move", fmt::format("item event: {:08X} x{} from {:08X} to {:08X}, unique id {}", a_event->baseObj, a_event->itemCount, a_event->oldContainer,
						a_event->newContainer, a_event->uniqueID));
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		std::string Describe(const RE::GFxValue& a_value)
		{
			constexpr std::size_t kLongest = 60;
			constexpr double kFormLike = 0x800;
			if (a_value.IsUndefined()) {
				return "undefined";
			}
			if (a_value.IsNull()) {
				return "null";
			}
			if (a_value.IsBool()) {
				return a_value.GetBool() ? "true" : "false";
			}
			if (a_value.IsNumber()) {
				const double number = a_value.GetNumber();
				const bool whole = number == std::floor(number) && number >= kFormLike && number <= 0xFFFFFFFF;
				return whole ? fmt::format("{:08X}", static_cast<std::uint32_t>(number)) : fmt::format("{}", number);
			}
			if (a_value.IsString()) {
				const char* text = a_value.GetString();
				return fmt::format("\"{}\"", std::string_view(text ? text : "").substr(0, kLongest));
			}
			if (a_value.IsArray()) {
				return fmt::format("[{} values]", a_value.GetArraySize());
			}
			return "an object";
		}
	}

	void Listen()
	{
		static Menus menus;
		static Equips equips;
		static Moves moves;
		RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&menus);
		auto* events = RE::ScriptEventSourceHolder::GetSingleton();
		events->AddEventSink<RE::TESEquipEvent>(&equips);
		events->AddEventSink<RE::TESContainerChangedEvent>(&moves);
	}

	void Message(const SKSE::MessagingInterface::Message& a_message)
	{
		using Kind = SKSE::MessagingInterface;
		const auto saveName = [&] {
			const auto* name = static_cast<const char*>(a_message.data);
			return name && a_message.dataLen > 0 ? std::string(name, strnlen(name, a_message.dataLen)) : std::string("?");
		};
		switch (a_message.type) {
		case Kind::kPostLoad:
			logger::info("SKSE: every plugin is loaded");
			break;
		case Kind::kPostPostLoad:
			logger::info("SKSE: every plugin heard the load");
			break;
		case Kind::kInputLoaded:
			logger::info("SKSE: the input is loaded");
			break;
		case Kind::kDataLoaded:
			logger::info("SKSE: the game data is loaded");
			break;
		case Kind::kNewGame:
			logger::info("SKSE: a new game starts");
			break;
		case Kind::kPreLoadGame:
			logger::info("SKSE: the save {} loads", saveName());
			break;
		case Kind::kPostLoadGame:
			logger::info("SKSE: a save loaded, {}", a_message.data ? "fine" : "it failed");
			break;
		case Kind::kSaveGame:
			logger::info("SKSE: the game saves as {}", saveName());
			break;
		case Kind::kDeleteGame:
			logger::info("SKSE: the save {} is deleted", saveName());
			break;
		default:
			logger::info("SKSE: message {}", a_message.type);
			break;
		}
	}

	NativeCall::NativeCall(const char* a_name, const RE::GFxFunctionHandler::Params& a_params) noexcept :
		name(a_name),
		params(a_params)
	{
		try {
			std::string arguments;
			for (std::uint32_t at = 0; at < a_params.argCount; ++at) {
				arguments += (at == 0 ? "" : ", ") + Describe(a_params.args[at]);
			}
			logger::debug("-> native {}({}) from {}", name, arguments, Script::KindOf(a_params.movie));
		} catch (...) {
		}
	}

	NativeCall::~NativeCall()
	{
		try {
			logger::debug("<- native {}: {}, {:.1f} ms", name, params.retVal ? Describe(*params.retVal) : std::string("nothing asked"),
				std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
		} catch (...) {
		}
	}
}
