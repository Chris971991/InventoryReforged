#include "PCH.h"

#include "Settings.h"

#include <cctype>
#include <cmath>

namespace Settings
{
	namespace
	{
		constexpr auto kShipped = "Data/SKSE/Plugins/InventoryReforged.ini";
		constexpr auto kSetInMenu = "Data/MCM/Settings/InventoryReforged.ini";
		// MCM Helper starts its file with this mark and the system ini reader loses the first section then
		constexpr std::string_view kByteOrderMark = "\xEF\xBB\xBF";
		constexpr std::string_view kBlank = " \t\r";
		// the switch of every slot of the panel in [Slots], in the order the panel numbers the slots
		constexpr std::array kSlotSwitches{ "bShowRight"sv, "bShowLeft"sv, "bShowAmmo"sv, "bShowAmulet"sv, "bShowRing"sv, "bShowHead"sv, "bShowBody"sv,
			"bShowHands"sv, "bShowFeet"sv, "bShowOther"sv };
		constexpr std::int32_t kLeastScale = 60;
		constexpr std::int32_t kMostScale = 140;
		constexpr std::int32_t kMostShift = 40;
		constexpr std::int32_t kNarrowest = 2;
		constexpr std::int32_t kLeastWindow = 70;
		constexpr std::int32_t kMostWindow = 160;
		constexpr std::int32_t kLastWindowShow = 2;

		Values current;

		// what a file says, under "section:name"
		using Lines = std::map<std::string, std::string>;

		// what both files say together, the menu reads its values from here. The menu script runs in another thread
		Lines merged;
		std::mutex mergedLock;

		std::string Trimmed(std::string_view a_text)
		{
			const auto first = a_text.find_first_not_of(kBlank);
			const auto last = a_text.find_last_not_of(kBlank);
			return first == std::string_view::npos ? std::string() : std::string(a_text.substr(first, last - first + 1));
		}

		Lines LinesOf(const std::filesystem::path& a_file)
		{
			Lines lines;
			std::ifstream stream(a_file);
			std::string section;
			for (std::string line; std::getline(stream, line);) {
				if (line.starts_with(kByteOrderMark)) {
					line.erase(0, kByteOrderMark.size());
				}
				line = Trimmed(line);
				const auto equals = line.find('=');
				if (line.starts_with('[') && line.ends_with(']')) {
					section = line.substr(1, line.size() - 2);
				} else if (!line.starts_with(';') && equals != std::string::npos) {
					lines[section + ':' + Trimmed(line.substr(0, equals))] = Trimmed(line.substr(equals + 1));
				}
			}
			return lines;
		}

		std::string Written(const Lines& a_lines, std::string_view a_section, std::string_view a_name)
		{
			const auto found = a_lines.find(std::string(a_section) + ':' + std::string(a_name));
			return found == a_lines.end() ? std::string() : found->second;
		}

		float Number(const Lines& a_lines, std::string_view a_section, std::string_view a_name, float a_fallback)
		{
			const auto text = Written(a_lines, a_section, a_name);
			char* end = nullptr;
			const float value = std::strtof(text.c_str(), &end);
			// strtof takes "nan" and "inf" too
			return end == text.c_str() || !std::isfinite(value) ? a_fallback : value;
		}

		std::int32_t Whole(const Lines& a_lines, std::string_view a_section, std::string_view a_name, std::int32_t a_fallback, std::int32_t a_least, std::int32_t a_most)
		{
			const float value = Number(a_lines, a_section, a_name, static_cast<float>(a_fallback));
			return std::clamp(static_cast<std::int32_t>(std::lround(value)), a_least, a_most);
		}

		bool Switch(const Lines& a_lines, std::string_view a_section, std::string_view a_name, bool a_fallback)
		{
			const auto text = Written(a_lines, a_section, a_name);
			if (text.empty()) {
				return a_fallback;
			}
			return text == "true" || Number(a_lines, a_section, a_name, 0.0F) != 0.0F;
		}

		// a colour as MCM writes it, a decimal number, or 0xD6B068 typed into the file by hand
		std::uint32_t Color(const Lines& a_lines, std::string_view a_section, std::string_view a_name, std::uint32_t a_fallback)
		{
			const auto text = Written(a_lines, a_section, a_name);
			char* end = nullptr;
			const auto value = std::strtoull(text.c_str(), &end, 0);
			return end == text.c_str() ? a_fallback : static_cast<std::uint32_t>(value & 0xFFFFFF);
		}

		void Read(Values& a_values, const Lines& a_lines)
		{
			a_values.panel = Switch(a_lines, "Panel", "bEnabled", a_values.panel);
			a_values.stillCamera = Switch(a_lines, "Panel", "bStillCamera", a_values.stillCamera);
			a_values.followers = Switch(a_lines, "Panel", "bFollowers", a_values.followers);
			a_values.companionTabs = Switch(a_lines, "Panel", "bCompanionTabs", a_values.companionTabs);
			a_values.magicMenu = Switch(a_lines, "Panel", "bMagicMenu", a_values.magicMenu);
			a_values.doll = Switch(a_lines, "Doll", "bShow", a_values.doll);
			a_values.pose = static_cast<Pose>(static_cast<std::int32_t>(Number(a_lines, "Doll", "iPose", static_cast<float>(a_values.pose))));
			a_values.turnSpeed = Number(a_lines, "Doll", "fTurnSpeed", a_values.turnSpeed);
			a_values.equipMotion = Switch(a_lines, "Doll", "bEquipMotion", a_values.equipMotion);
			a_values.slotModels = Switch(a_lines, "Slots", "bModels", a_values.slotModels);
			a_values.wornLook = Switch(a_lines, "Slots", "bWornLook", a_values.wornLook);
			a_values.hideIcons = Switch(a_lines, "Slots", "bHideIcons", a_values.hideIcons);
			a_values.popup = Switch(a_lines, "Slots", "bPopup", a_values.popup);
			a_values.doubleClickTakesOff = Switch(a_lines, "Slots", "bDoubleClickTakesOff", a_values.doubleClickTakesOff);
			a_values.dragAndDrop = Switch(a_lines, "Slots", "bDragAndDrop", a_values.dragAndDrop);
			a_values.pieceStrips = Switch(a_lines, "Slots", "bPieceStrips", a_values.pieceStrips);
			a_values.arrows = Switch(a_lines, "List", "bArrows", a_values.arrows);
			a_values.equalSign = Switch(a_lines, "List", "bEqualSign", a_values.equalSign);
			a_values.badges = Switch(a_lines, "List", "bBadges", a_values.badges);
			a_values.subcategories = Switch(a_lines, "List", "bSubcategories", a_values.subcategories);
			a_values.lastAdded = Switch(a_lines, "List", "bLastAdded", a_values.lastAdded);
			a_values.stackWeight = Switch(a_lines, "List", "bStackWeight", a_values.stackWeight);
			a_values.rowMenu = Switch(a_lines, "List", "bRowMenu", a_values.rowMenu);
			a_values.stackKey = static_cast<std::int32_t>(Number(a_lines, "List", "iStackKey", static_cast<float>(a_values.stackKey)));
			a_values.newMarks = Switch(a_lines, "List", "bNewMarks", a_values.newMarks);
			a_values.junk = Switch(a_lines, "List", "bJunk", a_values.junk);
			a_values.junkKey = static_cast<std::int32_t>(Number(a_lines, "List", "iJunkKey", static_cast<float>(a_values.junkKey)));
			a_values.weightWarnings = Switch(a_lines, "List", "bWeightWarnings", a_values.weightWarnings);
			a_values.questMarks = Switch(a_lines, "List", "bQuestMarks", a_values.questMarks);
			a_values.stolenApart = Switch(a_lines, "List", "bStolenApart", a_values.stolenApart);
			a_values.dollScale = Whole(a_lines, "Doll", "iScale", a_values.dollScale, kLeastScale, kMostScale);
			a_values.dollRight = Whole(a_lines, "Doll", "iShiftRight", a_values.dollRight, -kMostShift, kMostShift);
			a_values.dollUp = Whole(a_lines, "Doll", "iShiftUp", a_values.dollUp, -kMostShift, kMostShift);
			a_values.listWidth = Whole(a_lines, "Panel", "iListWidth", a_values.listWidth, 0, kNarrowest);
			for (std::uint32_t slot = 0; slot < kSlotSwitches.size(); ++slot) {
				const std::uint32_t bit = 1U << slot;
				const bool shown = Switch(a_lines, "Slots", kSlotSwitches[slot], (a_values.slotsOff & bit) == 0);
				a_values.slotsOff = shown ? a_values.slotsOff & ~bit : a_values.slotsOff | bit;
			}
			a_values.windowSize = Whole(a_lines, "Window", "iSize", a_values.windowSize, kLeastWindow, kMostWindow);
			a_values.windowShow = Whole(a_lines, "Window", "iShow", a_values.windowShow, 0, kLastWindowShow);
			a_values.stats = Switch(a_lines, "Window", "bStats", a_values.stats);
			a_values.heroStats = Switch(a_lines, "Window", "bHeroStats", a_values.heroStats);
			a_values.slotNumbers = Switch(a_lines, "Window", "bSlotNumbers", a_values.slotNumbers);
			a_values.quietTags = Switch(a_lines, "Window", "bQuietTags", a_values.quietTags);
			a_values.modOrigin = Switch(a_lines, "Window", "bModOrigin", a_values.modOrigin);
			a_values.questLine = Switch(a_lines, "Window", "bQuestLine", a_values.questLine);
			a_values.leaveCard = Switch(a_lines, "Window", "bLeaveCard", a_values.leaveCard);
			a_values.inspectOnZoomKey = Switch(a_lines, "Inspect", "bOnZoomKey", a_values.inspectOnZoomKey);
			a_values.accentColor = Color(a_lines, "Look", "iAccentColor", a_values.accentColor);
			a_values.textColor = Color(a_lines, "Look", "iTextColor", a_values.textColor);
			a_values.dumpPose = Switch(a_lines, "Debug", "bDumpPose", a_values.dumpPose);
#if !IR_DOVAKARN
			// Dovakarn: its build has no SKSE Menu Framework pages, so no window of them can be asked for (ModMenu.cpp)
			a_values.pagesWindow = Switch(a_lines, "Debug", "bPagesWindow", a_values.pagesWindow);
#endif
#if IR_DEBUG_EDITION
			a_values.stallSeconds = Whole(a_lines, "Debug", "iStallSeconds", a_values.stallSeconds, 0, 60);
#endif
		}
	}

	void Load()
	{
		Values read;
		auto lines = LinesOf(std::filesystem::current_path() / kShipped);
		const auto set = LinesOf(std::filesystem::current_path() / kSetInMenu);
		Read(read, lines);
		Read(read, set);
		current = read;
		for (const auto& [key, value] : set) {
			lines[key] = value;
		}
#if IR_DEBUG_EDITION
		std::string told;
		for (const auto& [key, value] : lines) {
			told += fmt::format("{}{} = {}", told.empty() ? "" : ", ", key, value);
		}
		logger::info("settings, {} of them from the menu's file: {}", set.size(), told);
#endif
		const std::scoped_lock lock{ mergedLock };
		merged = std::move(lines);
	}

	// "bModels:Slots" as the files key it, "Slots:bModels" in the letter case the shipped ini has. A script hands a
	// text over in the case the game met first, so the case of what it asks for doesn't count
	std::string KeyOf(std::string_view a_id)
	{
		const auto colon = a_id.find(':');
		const auto key = std::string(a_id.substr(colon + 1)) + ':' + std::string(a_id.substr(0, colon));
		const auto same = [&](const std::string& a_other) {
			return std::ranges::equal(a_other, key, [](char a_left, char a_right) { return std::tolower(static_cast<unsigned char>(a_left)) == std::tolower(static_cast<unsigned char>(a_right)); });
		};
		for (const auto& [known, value] : merged) {
			if (same(known)) {
				return known;
			}
		}
		return key;
	}

	float Value(std::string_view a_id)
	{
		const std::scoped_lock lock{ mergedLock };
		if (a_id.find(':') == std::string_view::npos) {
			return 0.0F;
		}
		const auto key = KeyOf(a_id);
		const auto colon = key.find(':');
		return Number(merged, key.substr(0, colon), key.substr(colon + 1), 0.0F);
	}

	void Set(std::string_view a_id, float a_value)
	{
		// a script or the menu may hand over nan or inf, casting those to a whole number is undefined
		if (a_id.find(':') == std::string_view::npos || !std::isfinite(a_value)) {
			return;
		}
		const std::scoped_lock lock{ mergedLock };
		const auto key = KeyOf(a_id);
		const auto text = a_value == std::floor(a_value) ? std::to_string(static_cast<std::int64_t>(a_value)) : std::format("{:.4g}", a_value);
		IR_LOG("setting {} goes from {} to {}", key, merged.contains(key) ? merged.at(key) : std::string("nothing"), text);
		const auto file = std::filesystem::current_path() / kSetInMenu;
		auto lines = LinesOf(file);
		lines[key] = text;
		merged[key] = text;
		std::error_code failed;
		std::filesystem::create_directories(file.parent_path(), failed);
		std::ofstream stream(file, std::ios::trunc);
		// the lines come sorted as "section:name", so every section is written once with all of its names
		std::string section;
		for (const auto& [where, value] : lines) {
			const auto at = where.find(':');
			if (where.substr(0, at) != section) {
				section = where.substr(0, at);
				stream << (stream.tellp() > 0 ? "\n[" : "[") << section << "]\n";
			}
			stream << where.substr(at + 1) << " = " << value << '\n';
		}
		if (!stream) {
			logger::warn("the menu couldn't write {}", kSetInMenu);
		}
	}

	void Change(std::string_view a_id, float a_value)
	{
		Set(a_id, a_value);
		Values read;
		{
			const std::scoped_lock lock{ mergedLock };
			Read(read, merged);
		}
		current = read;
	}

	const Values& Get()
	{
		return current;
	}

	void Fill(RE::GFxMovie& a_movie, RE::GFxValue& a_object)
	{
		const auto& values = current;
		a_movie.CreateObject(&a_object);
		a_object.SetMember("panel", values.panel);
		a_object.SetMember("doll", values.doll);
		a_object.SetMember("turnSpeed", values.turnSpeed);
		a_object.SetMember("slotModels", values.slotModels);
		a_object.SetMember("wornLook", values.wornLook);
		a_object.SetMember("hideIcons", values.hideIcons);
		a_object.SetMember("popup", values.popup);
		a_object.SetMember("doubleClickTakesOff", values.doubleClickTakesOff);
		a_object.SetMember("dragAndDrop", values.dragAndDrop);
		a_object.SetMember("pieceStrips", values.pieceStrips);
		a_object.SetMember("arrows", values.arrows);
		a_object.SetMember("equalSign", values.equalSign);
		a_object.SetMember("badges", values.badges);
		a_object.SetMember("subcategories", values.subcategories);
		a_object.SetMember("lastAdded", values.lastAdded);
		a_object.SetMember("stackWeight", values.stackWeight);
		a_object.SetMember("rowMenu", values.rowMenu);
		a_object.SetMember("stackKey", static_cast<double>(values.stackKey));
		a_object.SetMember("newMarks", values.newMarks);
		a_object.SetMember("junk", values.junk);
		a_object.SetMember("junkKey", static_cast<double>(values.junkKey));
		a_object.SetMember("weightWarnings", values.weightWarnings);
		a_object.SetMember("questMarks", values.questMarks);
		a_object.SetMember("dollScale", static_cast<double>(values.dollScale));
		a_object.SetMember("dollRight", static_cast<double>(values.dollRight));
		a_object.SetMember("dollUp", static_cast<double>(values.dollUp));
		a_object.SetMember("slotsOff", static_cast<double>(values.slotsOff));
		a_object.SetMember("listWidth", static_cast<double>(values.listWidth));
		a_object.SetMember("stats", values.stats);
		a_object.SetMember("heroStats", values.heroStats);
		a_object.SetMember("windowSize", static_cast<double>(values.windowSize));
		a_object.SetMember("windowShow", static_cast<double>(values.windowShow));
		a_object.SetMember("slotNumbers", values.slotNumbers);
		a_object.SetMember("quietTags", values.quietTags);
		a_object.SetMember("modOrigin", values.modOrigin);
		a_object.SetMember("questLine", values.questLine);
		a_object.SetMember("leaveCard", values.leaveCard);
		a_object.SetMember("inspectOnZoomKey", values.inspectOnZoomKey);
		a_object.SetMember("accent", static_cast<double>(values.accentColor));
		a_object.SetMember("text", static_cast<double>(values.textColor));
	}
}
