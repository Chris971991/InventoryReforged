#include "PCH.h"

#include "Debug/Actions.h"

#include <set>
#include <unordered_set>

#include "SafeRead.h"

namespace Debug::Actions
{
	namespace
	{
		// GASActionBufferData the way CommonLib lays it out, a block of a SWF's code with its bytes as the file has them
		constexpr std::size_t kBytesAt = 0x10;
		constexpr std::size_t kSizeAt = 0x18;
		constexpr std::size_t kLargest = 0x40000;
		// how much of an object a word of the stack points at gets looked through
		constexpr std::size_t kWindow = 0x100 / sizeof(std::uintptr_t);
		// ActionScript frames sit near the top, the rest of a deep stack is skipped to keep the look short
		constexpr std::size_t kMostWords = 0x8000;
		constexpr std::size_t kMostTold = 12;
		constexpr std::size_t kNamesTold = 10;
		constexpr std::size_t kLongestName = 40;
		constexpr std::uint8_t kEnd = 0x00;
		constexpr std::uint8_t kPool = 0x88;
		constexpr std::uint8_t kFunction = 0x9B;
		constexpr std::uint8_t kFunction2 = 0x8E;
		constexpr std::uint8_t kPush = 0x96;
		// the action codes of SWF 3 to 7
		constexpr std::array<std::pair<int, int>, 16> kCodes{ { { 0x00, 0x00 }, { 0x04, 0x15 }, { 0x17, 0x18 }, { 0x1C, 0x1D }, { 0x20, 0x2C }, { 0x30, 0x37 },
			{ 0x3A, 0x55 }, { 0x60, 0x69 }, { 0x81, 0x81 }, { 0x83, 0x83 }, { 0x87, 0x88 }, { 0x8A, 0x8F }, { 0x94, 0x94 }, { 0x96, 0x96 }, { 0x99, 0x9B },
			{ 0x9D, 0x9F } } };

		struct Record
		{
			std::uint32_t at;
			std::uint32_t end;
			std::uint8_t code;
		};

		struct Block
		{
			std::uintptr_t table = 0;
			std::size_t seen = 0;
			std::vector<std::uint8_t> bytes;
			std::vector<Record> records;
			std::vector<std::string> pool;
			// where the code of a function starts, to the record that makes the function
			std::unordered_map<std::uint32_t, std::size_t> functions;
		};

		struct Call
		{
			std::size_t seen;
			std::size_t block;
			std::size_t record;
		};

		bool Known(std::uint8_t a_code)
		{
			return std::ranges::any_of(kCodes, [&](const auto& a_range) { return a_code >= a_range.first && a_code <= a_range.second; });
		}

		std::uint32_t Word(const std::vector<std::uint8_t>& a_bytes, std::size_t a_at)
		{
			return a_at + 1 < a_bytes.size() ? static_cast<std::uint32_t>(a_bytes[a_at] | a_bytes[a_at + 1] << 8) : 0;
		}

		// the records from the first byte to the last one exactly, an end record only as the very last. Bytes that
		// aren't code almost never walk like that
		std::vector<Record> Walk(const std::vector<std::uint8_t>& a_bytes)
		{
			std::vector<Record> records;
			for (std::size_t at = 0; at < a_bytes.size();) {
				const auto code = a_bytes[at];
				const std::size_t end = code < 0x80 ? at + 1 : at + 3 + Word(a_bytes, at + 1);
				if (!Known(code) || end > a_bytes.size() || (code == kEnd && end != a_bytes.size())) {
					return {};
				}
				records.push_back({ static_cast<std::uint32_t>(at), static_cast<std::uint32_t>(end), code });
				at = end;
			}
			return records.size() >= 2 ? records : std::vector<Record>{};
		}

		std::string Text(const std::vector<std::uint8_t>& a_bytes, std::size_t& a_at, std::size_t a_end)
		{
			std::string text;
			for (; a_at < a_end && a_bytes[a_at] != 0; ++a_at) {
				text += a_bytes[a_at] >= 0x20 && a_bytes[a_at] < 0x7F ? static_cast<char>(a_bytes[a_at]) : '?';
			}
			++a_at;
			return text;
		}

		std::uint32_t CodeSize(const Block& a_block, std::size_t a_record)
		{
			return Word(a_block.bytes, a_block.records[a_record].end - 2);
		}

		// a block of code is an object of the game that holds where its bytes are and how many, and the bytes walk as
		// code right to their end
		std::optional<Block> BlockAt(std::uintptr_t a_at, std::uintptr_t a_gameStart, std::uintptr_t a_gameEnd, SafeRead::Regions& a_regions)
		{
			std::array<std::uintptr_t, 4> head{};
			if ((a_at & 7) != 0 || !SafeRead::Copy(head.data(), a_at, sizeof(head), a_regions) || head[0] < a_gameStart || head[0] >= a_gameEnd) {
				return std::nullopt;
			}
			const auto references = static_cast<std::uint32_t>(head[1]);
			const auto size = head[kSizeAt / sizeof(std::uintptr_t)];
			if (references == 0 || references > 0x100000 || size < 2 || size > kLargest) {
				return std::nullopt;
			}
			Block block{ .table = head[0], .bytes = std::vector<std::uint8_t>(size) };
			if (!SafeRead::Copy(block.bytes.data(), head[kBytesAt / sizeof(std::uintptr_t)], size, a_regions)) {
				return std::nullopt;
			}
			block.records = Walk(block.bytes);
			if (block.records.empty()) {
				return std::nullopt;
			}
			if (block.records[0].code == kPool) {
				for (std::size_t at = block.records[0].at + 5; at < block.records[0].end;) {
					block.pool.push_back(Text(block.bytes, at, block.records[0].end));
				}
			}
			for (std::size_t at = 0; at < block.records.size(); ++at) {
				if (block.records[at].code == kFunction || block.records[at].code == kFunction2) {
					block.functions.emplace(block.records[at].end, at);
				}
			}
			return block;
		}

		// the last name a push puts on the stack, it's the member a function gets set to right after
		std::string LastPushed(const Block& a_block, const Record& a_push)
		{
			std::string last;
			for (std::size_t at = a_push.at + 3; at < a_push.end;) {
				const auto type = a_block.bytes[at++];
				last.clear();
				switch (type) {
				case 0:
					last = Text(a_block.bytes, at, a_push.end);
					break;
				case 8:
					if (at < a_push.end && a_block.bytes[at] < a_block.pool.size()) {
						last = a_block.pool[a_block.bytes[at]];
					}
					at += 1;
					break;
				case 9:
					if (const auto index = Word(a_block.bytes, at); index < a_block.pool.size()) {
						last = a_block.pool[index];
					}
					at += 2;
					break;
				case 1:
				case 7:
					at += 4;
					break;
				case 6:
					at += 8;
					break;
				case 4:
				case 5:
					at += 1;
					break;
				case 2:
				case 3:
					break;
				default:
					return {};
				}
			}
			return last;
		}

		std::string FunctionName(const Block& a_block, std::size_t a_record)
		{
			const auto& record = a_block.records[a_record];
			const bool second = record.code == kFunction2;
			std::size_t at = record.at + 3;
			const std::size_t end = record.end - 2;
			auto name = Text(a_block.bytes, at, end);
			const auto count = Word(a_block.bytes, at);
			at += second ? 5 : 2;
			std::string parameters;
			for (std::uint32_t i = 0; i < count && at < end; ++i) {
				at += second ? 1 : 0;
				parameters += (i == 0 ? "" : ", ") + Text(a_block.bytes, at, end);
			}
			if (name.empty() && a_record > 0 && a_block.records[a_record - 1].code == kPush) {
				name = LastPushed(a_block, a_block.records[a_record - 1]);
			}
			return fmt::format("{}({})", name.empty() ? "a function" : name, parameters);
		}

		// The block by the first names of its code, they say the class or the part it comes from. The hash finds it in
		// the SWF files later if the names aren't enough
		std::string Where(const Block& a_block)
		{
			std::uint64_t hash = 0xCBF29CE484222325;
			for (const auto byte : a_block.bytes) {
				hash = (hash ^ byte) * 0x100000001B3;
			}
			std::string names;
			for (std::size_t at = 0; at < a_block.pool.size() && at < kNamesTold; ++at) {
				names += (at == 0 ? "" : ", ") + a_block.pool[at].substr(0, kLongestName);
			}
			return fmt::format("code {:016X} of {} bytes, names {}", hash, a_block.bytes.size(), names.empty() ? "none" : names);
		}
	}

	std::string OnStack(std::span<const std::uintptr_t> a_words, std::uintptr_t a_gameStart, std::uintptr_t a_gameEnd)
	{
		try {
			const auto words = a_words.first(std::min(a_words.size(), kMostWords));
			SafeRead::Regions regions;
			std::vector<Block> blocks;
			std::unordered_map<std::uintptr_t, std::size_t> blockAt;
			std::unordered_set<std::uintptr_t> looked;
			std::array<std::uintptr_t, kWindow> window{};
			const auto look = [&](std::uintptr_t a_at, std::size_t a_seen) {
				if (!looked.insert(a_at).second) {
					return;
				}
				if (auto block = BlockAt(a_at, a_gameStart, a_gameEnd, regions)) {
					block->seen = a_seen;
					blockAt.emplace(a_at, blocks.size());
					blocks.push_back(std::move(*block));
				}
			};
			// the blocks a word of the stack leads to, itself or one of the fields it points at
			for (std::size_t at = 0; at < words.size(); ++at) {
				look(words[at], at);
				if ((words[at] & 7) == 0 && SafeRead::Copy(window.data(), words[at], sizeof(window), regions)) {
					for (const auto field : window) {
						look(field, at);
					}
				}
			}
			// a field that leads to a block, straight or through one more object, then where the function's code
			// starts and how long it is. Only a pair that fits a function record of that block counts
			const auto blockOf = [&](std::uintptr_t a_at) -> std::optional<std::size_t> {
				if (const auto found = blockAt.find(a_at); found != blockAt.end()) {
					return found->second;
				}
				std::array<std::uintptr_t, 8> holder{};
				if ((a_at & 7) == 0 && SafeRead::Copy(holder.data(), a_at, sizeof(holder), regions)) {
					for (const auto field : holder) {
						if (const auto found = blockAt.find(field); found != blockAt.end()) {
							return found->second;
						}
					}
				}
				return std::nullopt;
			};
			std::vector<Call> calls;
			for (std::size_t at = 0; at < words.size() && !blocks.empty(); ++at) {
				if ((words[at] & 7) != 0 || !SafeRead::Copy(window.data(), words[at], sizeof(window), regions)) {
					continue;
				}
				for (std::size_t field = 0; field + 1 < window.size(); ++field) {
					const auto pair = window[field + 1];
					const auto low = static_cast<std::uint32_t>(pair);
					const auto high = static_cast<std::uint32_t>(pair >> 32);
					for (const auto [start, length] : { std::pair{ low, high }, std::pair{ high, low } }) {
						if (start == 0 || length == 0 || start > kLargest || length > kLargest) {
							continue;
						}
						const auto block = blockOf(window[field]);
						if (!block) {
							break;
						}
						const auto found = blocks[*block].functions.find(start);
						if (found != blocks[*block].functions.end() && CodeSize(blocks[*block], found->second) == length) {
							calls.push_back({ at, *block, found->second });
						}
					}
				}
			}

			static std::once_flag toldTable;
			if (!blocks.empty()) {
				std::call_once(toldTable, [&] { logger::info("blocks of ActionScript code of the game have their table at SkyrimSE.exe+{:X}", blocks[0].table - a_gameStart); });
			}

			std::ranges::stable_sort(calls, {}, &Call::seen);
			std::set<std::pair<std::size_t, std::size_t>> said;
			std::set<std::size_t> blocksSaid;
			std::string told;
			std::size_t count = 0;
			for (const auto& call : calls) {
				if (count < kMostTold && said.emplace(call.block, call.record).second) {
					blocksSaid.insert(call.block);
					told += fmt::format("{}[{}] function {} in {}", told.empty() ? "" : "; ", ++count, FunctionName(blocks[call.block], call.record), Where(blocks[call.block]));
				}
			}
			for (std::size_t at = 0; at < blocks.size() && count < kMostTold; ++at) {
				if (!blocksSaid.contains(at)) {
					told += fmt::format("{}[{}] {}", told.empty() ? "" : "; ", ++count, Where(blocks[at]));
				}
			}
			return told.empty() ? "no ActionScript found one step from the main thread's stack" : told;
		} catch (...) {
			return "could not be read";
		}
	}
}
