#include "PCH.h"

#include "Translations.h"

#include <cstring>
#include <sstream>

namespace Translations
{
	namespace
	{
		constexpr auto kFolder = "Data/Interface/Translations/";

		using Lines = std::vector<std::pair<std::string, std::wstring>>;
		// the keys are plain ASCII, the words can be any language
		Lines lines;
		std::string prefix;
		// false keeps the mod off the game's translator altogether, the words other mods give their keys come from
		// their files then
		bool intoGame = true;
		std::unordered_map<std::string, std::wstring> others;
		bool othersRead = false;

		std::string Language()
		{
			const auto* setting = RE::INISettingCollection::GetSingleton()->GetSetting("sLanguage:General");
			const char* told = setting ? setting->GetString() : nullptr;
			return told && *told ? told : "ENGLISH";
		}

		// a file of UTF-16 as text, its byte order mark left out
		std::wstring Wide(const std::string& a_bytes)
		{
			std::wstring text(a_bytes.size() / 2, L'\0');
			std::memcpy(text.data(), a_bytes.data(), text.size() * 2);
			if (!text.empty() && text.front() == L'\xFEFF') {
				text.erase(0, 1);
			}
			return text;
		}

		std::wstring Read(const std::filesystem::path& a_file)
		{
			std::ifstream stream(a_file, std::ios::binary);
			return Wide(std::string{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() });
		}

		// a file as the game finds it, loose or in an archive
		std::string ReadGameFile(const std::string& a_path)
		{
			RE::BSResourceNiBinaryStream stream(a_path);
			std::string bytes;
			if (!stream.good()) {
				return bytes;
			}
			std::array<char, 4096> chunk{};
			for (bool full = true; full;) {
				const auto before = stream.tell();
				full = stream.read(chunk.data(), static_cast<std::uint32_t>(chunk.size()));
				const auto got = stream.tell() - before;
				bytes.append(chunk.data(), got);
				full = full && got > 0;
			}
			return bytes;
		}

		Lines Parse(const std::wstring& a_text)
		{
			Lines parsed;
			std::wistringstream rows(a_text);
			for (std::wstring row; std::getline(rows, row);) {
				if (!row.empty() && row.back() == L'\r') {
					row.pop_back();
				}
				const auto tab = row.find(L'\t');
				if (tab == std::wstring::npos || tab == 0) {
					continue;
				}
				std::string key;
				for (const auto letter : row.substr(0, tab)) {
					key.push_back(static_cast<char>(letter < 128 ? letter : '?'));
				}
				parsed.emplace_back(std::move(key), row.substr(tab + 1));
			}
			return parsed;
		}

		// The words other mods give their own keys, "$TNG_TCT", from the translation file of each plugin of the load
		// order, the files SKSE reads into the game's translator. Read on the first ask
		void ReadOthers()
		{
			if (std::exchange(othersRead, true)) {
				return;
			}
			const auto language = Language();
			std::size_t files = 0;
			for (const auto* file : RE::TESDataHandler::GetSingleton()->files) {
				if (!file || file->compileIndex == 0xFF) {
					continue;
				}
				const auto name = std::string(file->GetFilename());
				const auto dot = name.rfind('.');
				const auto bytes = ReadGameFile(std::format("Interface\\Translations\\{}_{}.txt", name.substr(0, dot), language));
				if (bytes.empty()) {
					continue;
				}
				files++;
				for (auto& [key, words] : Parse(Wide(bytes))) {
					others.emplace(std::move(key), std::move(words));
				}
			}
			logger::info("words of other mods: {} keys from {} translation files", others.size(), files);
		}

		// The game's own translator, none when the game has none yet or a mod put one of its own in its place
		RE::BSScaleformTranslator* GameTranslator()
		{
			auto* manager = RE::BSScaleformManager::GetSingleton();
			auto* loader = manager ? manager->loader : nullptr;
			auto* translator = loader ? loader->GetStateAddRef<RE::BSScaleformTranslator>(RE::GFxState::StateType::kTranslator) : nullptr;
			if (!translator) {
				return nullptr;
			}
			const REL::Relocation<std::uintptr_t> own{ RE::VTABLE_BSScaleformTranslator[0] };
			if (*reinterpret_cast<const std::uintptr_t*>(translator) != own.address()) {
				static bool told = false;
				if (!std::exchange(told, true)) {
					logger::warn("a mod put a translator of its own in the game's place, its words are left alone");
				}
				translator->Release();
				return nullptr;
			}
			return translator;
		}

		// for the menus, which only read the game's translator
		void Translator(const std::filesystem::path& a_file)
		{
			auto* translator = GameTranslator();
			if (!translator) {
				logger::warn("no translator of the game to take the words of {}, the menus have them from SKSE or show their keys", a_file.string());
				return;
			}
			auto& words = translator->translator.translationMap;
			// SKSE read the file for a plugin of the load order, the game's translator has every line then
			const std::wstring first(lines.front().first.begin(), lines.front().first.end());
			if (words.find(RE::BSFixedStringW(first.c_str())) != words.end()) {
				translator->Release();
				logger::info("words of {}: {} lines, the game's translator has them from SKSE", a_file.string(), lines.size());
				return;
			}
			std::size_t added = 0;
			for (const auto& [key, text] : lines) {
				const std::wstring wide(key.begin(), key.end());
				added += words.emplace(RE::BSFixedStringW(wide.c_str()), RE::BSFixedStringW(text.c_str())).second ? 1 : 0;
			}
			translator->Release();
			logger::info("words of {}: {} lines, {} put into the game's translator, {} it had already", a_file.string(), lines.size(), added, lines.size() - added);
		}
	}

	void Load(std::string_view a_stem, std::string_view a_prefix, bool a_intoGame)
	{
		prefix = a_prefix;
		intoGame = a_intoGame;
		const auto named = std::format("{}{}_", kFolder, a_stem);
		std::filesystem::path file = named + Language() + ".txt";
		if (!std::filesystem::exists(file)) {
			file = named + "ENGLISH.txt";
		}
		lines = Parse(Read(file));
		if (lines.empty()) {
			logger::warn("no words in {}, the mod speaks English", file.string());
			return;
		}
		if (!intoGame) {
			logger::info("words of {}: {} lines, kept off the game's translator", file.string(), lines.size());
			return;
		}
		Translator(file);
	}

	std::string Game(std::string_view a_key)
	{
		if (!intoGame) {
			ReadOthers();
			const auto found = others.find(std::string(a_key));
			return found != others.end() ? SKSE::stl::utf16_to_utf8(found->second).value_or(std::string()) : std::string();
		}
		auto* translator = GameTranslator();
		if (!translator) {
			return {};
		}
		const auto wide = SKSE::stl::utf8_to_utf16(a_key).value_or(std::wstring());
		const auto& words = translator->translator.translationMap;
		const auto found = words.find(RE::BSFixedStringW(wide.c_str()));
		std::string text;
		if (found != words.end() && found->second.c_str()) {
			text = SKSE::stl::utf16_to_utf8(found->second.c_str()).value_or(std::string());
		}
		translator->Release();
		return text;
	}

	std::string Word(std::string_view a_key)
	{
		for (const auto& [key, text] : lines) {
			if (key == a_key) {
				return SKSE::stl::utf16_to_utf8(text).value_or(std::string());
			}
		}
		return std::string(!prefix.empty() && a_key.starts_with(prefix) ? a_key.substr(prefix.size()) : a_key);
	}

	void Fill(RE::GFxMovie& a_movie, RE::GFxValue& a_words)
	{
		a_movie.CreateObject(&a_words);
		for (const auto& [key, text] : lines) {
			RE::GFxValue words;
			a_movie.CreateStringW(&words, text.c_str());
			a_words.SetMember(key.c_str(), words);
		}
	}
}
