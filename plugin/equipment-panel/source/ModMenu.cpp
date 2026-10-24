#include "PCH.h"

#include "ModMenu.h"

#include <nlohmann/json.hpp>

#include "Hidden.h"
#include "Settings.h"
#include "Subject.h"
#include "Translations.h"

#include "SKSEMenuFramework.h"

namespace ModMenu
{
	namespace
	{
		constexpr auto kPages = "Data/SKSE/Plugins/InventoryReforged/pages.json";
		// a page is drawn by a function of its own, the framework hands it nothing to tell them apart
		constexpr std::size_t kMostPages = 6;
		// the help of a control wraps at this many letters
		constexpr float kHelpLetters = 38.0F;
		constexpr std::uint32_t kEscape = 0x01;
		constexpr std::uint32_t kDelete = 0xD3;

		// One line of a page as the MCM pages have it: a header, a switch, a slider, a list of choices, a colour, a key,
		// or words with a press that calls a function of the menu script
		struct Control
		{
			std::string kind;
			std::string id;
			std::string text;
			std::string help;
			float least = 0.0F;
			float most = 1.0F;
			bool whole = true;
			std::string format;
			std::vector<std::string> options;
			std::string action;
		};

		struct Page
		{
			std::string name;
			std::vector<Control> controls;
		};

		std::vector<Page> pages;

		// a slider or a colour being dragged shows what it would be and is written once let go
		std::unordered_map<std::string, float> dragged;

		// the key control that waits for the next key the player presses
		std::string waiting;
		std::mutex waitingLock;

		// MCM writes "{0}%" for a whole number with a percent sign after it, "{2}" for two digits after the point
		std::string FormatOf(std::string_view a_mcm, bool a_whole)
		{
			std::string format;
			for (std::size_t i = 0; i < a_mcm.size(); ++i) {
				if (a_mcm[i] == '{' && i + 2 < a_mcm.size() && a_mcm[i + 2] == '}') {
					format += a_whole ? std::string("%d") : std::format("%.{}f", a_mcm[i + 1]);
					i += 2;
				} else if (a_mcm[i] == '%') {
					format += "%%";
				} else {
					format += a_mcm[i];
				}
			}
			return format.empty() ? std::string(a_whole ? "%d" : "%.2f") : format;
		}

		std::string Said(const nlohmann::json& a_item, const char* a_name)
		{
			return a_item.contains(a_name) && a_item[a_name].is_string() ? Translations::Word(a_item[a_name].get<std::string>()) : std::string();
		}

		Control ControlOf(const nlohmann::json& a_item)
		{
			Control control;
			control.kind = a_item.value("type", "");
			control.id = a_item.value("id", "");
			control.text = Said(a_item, "text");
			control.help = Said(a_item, "help");
			const auto options = a_item.value("valueOptions", nlohmann::json::object());
			control.least = options.value("min", 0.0F);
			control.most = options.value("max", 1.0F);
			control.whole = options.value("sourceType", "") != "ModSettingFloat";
			control.format = FormatOf(options.value("formatString", ""), control.whole);
			for (const auto& choice : options.value("options", nlohmann::json::array())) {
				control.options.push_back(choice.is_string() ? Translations::Word(choice.get<std::string>()) : std::string());
			}
			if (a_item.contains("action")) {
				control.action = a_item["action"].value("function", "");
			}
			return control;
		}

		void Read()
		{
			std::ifstream stream(std::filesystem::current_path() / kPages);
			const auto read = nlohmann::json::parse(stream, nullptr, false);
			if (read.is_discarded() || !read.contains("pages")) {
				logger::warn("no pages for SKSE Menu Framework in {}", kPages);
				return;
			}
			// a field of the wrong type throws, and out of kDataLoaded that took the whole game down at load
			try {
				for (const auto& page : read["pages"]) {
					Page made{ Said(page, "pageDisplayName") };
					for (const auto& item : page.value("content", nlohmann::json::array())) {
						made.controls.push_back(ControlOf(item));
					}
					pages.push_back(std::move(made));
				}
			} catch (const std::exception& e) {
				logger::warn("the pages for SKSE Menu Framework in {} can't be read, the menu goes without them: {}", kPages, e.what());
				pages.clear();
			}
		}

		// the menu script's functions the words of a page call
		void Call(std::string_view a_function)
		{
			if (a_function == "ShowEverything") {
				SKSE::GetTaskInterface()->AddTask([] { Hidden::ShowAll(); });
			} else if (a_function == "DressFollowersAgain") {
				SKSE::GetTaskInterface()->AddTask([] { Subject::ForgetReadOnly(); });
			}
		}

		void Changed(const std::string& a_id, float a_value)
		{
			Settings::Set(a_id, a_value);
			SKSE::GetTaskInterface()->AddTask([] { Reload(); });
		}

		// the help of the control just drawn, under the pointer
		void Help(const Control& a_control)
		{
			if (a_control.help.empty() || !ImGuiMCP::IsItemHovered()) {
				return;
			}
			ImGuiMCP::BeginTooltip();
			ImGuiMCP::PushTextWrapPos(ImGuiMCP::GetFontSize() * kHelpLetters);
			ImGuiMCP::TextUnformatted(a_control.help.c_str());
			ImGuiMCP::PopTextWrapPos();
			ImGuiMCP::EndTooltip();
		}

		std::string KeyName(std::uint32_t a_code)
		{
			static const std::unordered_map<std::uint32_t, std::string_view> names{
				{ 0x01, "Esc" }, { 0x0C, "-" }, { 0x0D, "=" }, { 0x0E, "Backspace" }, { 0x0F, "Tab" }, { 0x1A, "[" }, { 0x1B, "]" },
				{ 0x1C, "Enter" }, { 0x1D, "Left Ctrl" }, { 0x27, ";" }, { 0x28, "'" }, { 0x29, "`" }, { 0x2A, "Left Shift" },
				{ 0x2B, "\\" }, { 0x33, "," }, { 0x34, "." }, { 0x35, "/" }, { 0x36, "Right Shift" }, { 0x37, "Num *" },
				{ 0x38, "Left Alt" }, { 0x39, "Space" }, { 0x3A, "Caps Lock" }, { 0x45, "Num Lock" }, { 0x46, "Scroll Lock" },
				{ 0x4A, "Num -" }, { 0x4E, "Num +" }, { 0x53, "Num ." }, { 0x57, "F11" }, { 0x58, "F12" }, { 0x9C, "Num Enter" },
				{ 0x9D, "Right Ctrl" }, { 0xB5, "Num /" }, { 0xB8, "Right Alt" }, { 0xC7, "Home" }, { 0xC8, "Up" }, { 0xC9, "Page Up" },
				{ 0xCB, "Left" }, { 0xCD, "Right" }, { 0xCF, "End" }, { 0xD0, "Down" }, { 0xD1, "Page Down" }, { 0xD2, "Insert" },
				{ 0xD3, "Delete" }
			};
			// the rows of letters and digits the way the keyboard has them
			constexpr std::string_view kRows = "1234567890----QWERTYUIOP----ASDFGHJKL-----ZXCVBNM";
			constexpr std::string_view kNumbers = "789-456+1230";
			if (const auto named = names.find(a_code); named != names.end()) {
				return std::string(named->second);
			}
			if (a_code >= 0x02 && a_code < 0x02 + kRows.size() && kRows[a_code - 0x02] != '-') {
				return std::string(1, kRows[a_code - 0x02]);
			}
			if (a_code >= 0x3B && a_code <= 0x44) {
				return std::format("F{}", a_code - 0x3A);
			}
			if (a_code >= 0x47 && a_code < 0x47 + kNumbers.size()) {
				return std::format("Num {}", kNumbers[a_code - 0x47]);
			}
			return a_code == 0 ? std::string("-") : std::format("Key {}", a_code);
		}

		// the next key pressed while a key control waits becomes its key, Esc lets it go and Delete leaves it with none
		bool __stdcall Pressed(RE::InputEvent* a_event)
		{
			const std::scoped_lock lock{ waitingLock };
			const auto* button = a_event ? a_event->AsButtonEvent() : nullptr;
			if (waiting.empty() || !button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard || !button->IsDown()) {
				return false;
			}
			const auto code = button->GetIDCode();
			if (code != kEscape) {
				Changed(waiting, code == kDelete ? 0.0F : static_cast<float>(code));
			}
			waiting.clear();
			return true;
		}

		float Shown(const Control& a_control)
		{
			const auto at = dragged.find(a_control.id);
			return at != dragged.end() ? at->second : Settings::Value(a_control.id);
		}

		// a slider or a colour is written once it's let go, the file isn't written on every frame of a drag
		void Dragged(const Control& a_control, bool a_moved, float a_value)
		{
			if (a_moved) {
				dragged[a_control.id] = a_value;
			}
			if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
				Changed(a_control.id, a_value);
				dragged.erase(a_control.id);
			} else if (!ImGuiMCP::IsItemActive()) {
				dragged.erase(a_control.id);
			}
		}

		void DrawColor(const Control& a_control)
		{
			const auto color = static_cast<std::uint32_t>(Shown(a_control));
			float parts[3]{ ((color >> 16) & 0xFF) / 255.0F, ((color >> 8) & 0xFF) / 255.0F, (color & 0xFF) / 255.0F };
			const bool moved = ImGuiMCP::ColorEdit3(a_control.text.c_str(), parts);
			const auto part = [&](int a_index) { return static_cast<std::uint32_t>(std::lround(std::clamp(parts[a_index], 0.0F, 1.0F) * 255.0F)); };
			Dragged(a_control, moved, static_cast<float>(part(0) << 16 | part(1) << 8 | part(2)));
		}

		void DrawKey(const Control& a_control)
		{
			bool asking;
			{
				const std::scoped_lock lock{ waitingLock };
				asking = waiting == a_control.id;
			}
			const auto shown = asking ? Translations::Word("$IR_Press a key, Esc keeps it, Delete clears it") : KeyName(static_cast<std::uint32_t>(Shown(a_control)));
			if (ImGuiMCP::Button(std::format("{}##key", shown).c_str())) {
				const std::scoped_lock lock{ waitingLock };
				waiting = asking ? std::string() : a_control.id;
			}
			ImGuiMCP::SameLine();
			ImGuiMCP::TextUnformatted(a_control.text.c_str());
		}

		void Draw(const Control& a_control)
		{
			if (a_control.kind == "header") {
				ImGuiMCP::SeparatorText(a_control.text.c_str());
				return;
			}
			ImGuiMCP::PushID(a_control.id.empty() ? a_control.text.c_str() : a_control.id.c_str());
			if (a_control.kind == "toggle") {
				bool on = Shown(a_control) != 0.0F;
				if (ImGuiMCP::Checkbox(a_control.text.c_str(), &on)) {
					Changed(a_control.id, on ? 1.0F : 0.0F);
				}
			} else if (a_control.kind == "slider" && a_control.whole) {
				int value = static_cast<int>(std::lround(Shown(a_control)));
				const bool moved = ImGuiMCP::SliderInt(a_control.text.c_str(), &value, static_cast<int>(a_control.least), static_cast<int>(a_control.most), a_control.format.c_str());
				Dragged(a_control, moved, static_cast<float>(value));
			} else if (a_control.kind == "slider") {
				float value = Shown(a_control);
				const bool moved = ImGuiMCP::SliderFloat(a_control.text.c_str(), &value, a_control.least, a_control.most, a_control.format.c_str());
				Dragged(a_control, moved, value);
			} else if (a_control.kind == "enum") {
				int chosen = static_cast<int>(std::lround(Shown(a_control)));
				std::vector<const char*> choices;
				for (const auto& option : a_control.options) {
					choices.push_back(option.c_str());
				}
				if (ImGuiMCP::Combo(a_control.text.c_str(), &chosen, choices.data(), static_cast<int>(choices.size()))) {
					Changed(a_control.id, static_cast<float>(chosen));
				}
			} else if (a_control.kind == "color") {
				DrawColor(a_control);
			} else if (a_control.kind == "keymap") {
				DrawKey(a_control);
			} else if (!a_control.action.empty()) {
				if (ImGuiMCP::Button(a_control.text.c_str())) {
					Call(a_control.action);
				}
			} else {
				ImGuiMCP::TextWrapped("%s", a_control.text.c_str());
			}
			Help(a_control);
			ImGuiMCP::PopID();
		}

		template <std::size_t N>
		void __stdcall DrawPage()
		{
			if (N < pages.size()) {
				for (const auto& control : pages[N].controls) {
					Draw(control);
				}
			}
		}

		// for the test stand: every page in one window, the stand can't press the framework's key or its section
		void __stdcall DrawWindow()
		{
			constexpr float kWide = 1100.0F;
			constexpr float kHigh = 1000.0F;
			ImGuiMCP::SetNextWindowPos(ImGuiMCP::ImVec2(20.0F, 20.0F), ImGuiMCP::ImGuiCond_FirstUseEver);
			ImGuiMCP::SetNextWindowSize(ImGuiMCP::ImVec2(kWide, kHigh), ImGuiMCP::ImGuiCond_FirstUseEver);
			if (ImGuiMCP::Begin("Inventory Reforged##pages")) {
				for (const auto& page : pages) {
					ImGuiMCP::SeparatorText(page.name.c_str());
					for (const auto& control : page.controls) {
						Draw(control);
					}
				}
			}
			ImGuiMCP::End();
		}

		template <std::size_t... N>
		void AddPages(std::index_sequence<N...>)
		{
			constexpr std::array<SKSEMenuFramework::Model::RenderFunction, sizeof...(N)> drawn{ DrawPage<N>... };
			for (std::size_t i = 0; i < pages.size() && i < drawn.size(); ++i) {
				SKSEMenuFramework::AddSectionItem(pages[i].name, drawn[i]);
			}
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			return;
		}
		Read();
		if (pages.empty()) {
			return;
		}
		SKSEMenuFramework::SetSection("Inventory Reforged");
		AddPages(std::make_index_sequence<kMostPages>());
		SKSEMenuFramework::AddInputEvent(Pressed);
		logger::info("SKSE Menu Framework has the menu of the mod, {} pages", pages.size());
		if (Settings::Get().pagesWindow) {
			if (auto* window = SKSEMenuFramework::AddWindow(DrawWindow, false)) {
				window->IsOpen = true;
				logger::info("the pages are open in a window of their own for the test stand");
			}
		}
	}

	void Reload()
	{
		const bool panel = Settings::Get().panel;
		Settings::Load();
		if (panel != Settings::Get().panel) {
			Hidden::Refresh();
		}
	}
}
