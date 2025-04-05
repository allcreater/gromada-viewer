module;

#if __INTELLISENSE__
#include <array>
#include <bitset>
#include <filesystem>
#include <functional>
#include <ranges>
#include <mdspan>
#endif

#include <cassert>
//#include <json/json.h>

export module Gromada.Resources;

import Gromada.ResourceReader;
import std;
import utils;

export enum class UnitType : std::uint8_t {
	Terrain = 0x1,
	Object = 0x2,
	Monster = 0x4,
	Avia = 0x8,
	Cannon = 0x10,
	Sprite = 0x20,
	Item = 0x40,
};

export struct RGBA8 {
	std::uint8_t r;
	std::uint8_t g;
	std::uint8_t b;
	std::uint8_t a;
};
export struct VidGraphics {
	std::uint8_t visualBehavior;
	std::uint16_t hz7;
	std::uint16_t numOfFrames;
	std::uint32_t dataSize;
	std::uint16_t imgWidth;
	std::uint16_t imgHeight;

	std::array<std::uint8_t, 0x300> palette;
	std::vector<std::byte> data;
	std::vector<std::span<std::byte>> frames;

	// TODO: move out of there
	RGBA8 getPaletteColor(std::uint8_t index) const {
		auto offset = index * 3;
		return {palette[offset], palette[offset + 1], palette[offset + 2], 255};
	}

	void read(BinaryStreamReader& reader);

	using DecodedData = std::vector<std::vector<RGBA8>>;
	DecodedData decode() const;

private:
	void decodeFormat0(DecodedData& out) const;
	void decodeFormat2(DecodedData& out) const;
};

export struct VidRawData {
	std::array<char, 34> name;
	UnitType unitType;
	std::uint8_t behave;
	std::uint16_t flags;

	std::uint8_t collisionMask;
	std::uint16_t anotherWidth;
	std::uint16_t anotherHeight;
	std::uint16_t z_or_height;
	std::uint8_t maxHP;
	std::uint16_t gridRadius;
	std::uint8_t p6;
	std::uint16_t speed;

	std::uint16_t hz1;
	std::uint16_t hz2;
	std::uint8_t hz3;
	std::uint8_t army;
	std::uint8_t someWeaponIndex;
	std::uint8_t hz4;
	std::uint16_t deathSizeMargin;
	std::uint8_t somethingAboutDeath;
	std::uint8_t sX;
	std::uint8_t sY;
	std::uint8_t sZ;

	std::uint16_t hz5;
	std::uint16_t hz6;
	std::uint8_t direction;
	std::uint8_t z;

	std::array<std::uint8_t, 144> supportedActions;
	std::array<std::uint16_t, 16> children;
	std::array<std::uint8_t, 16> something;

	std::int32_t dataSizeOrNvid; // if < 0 then it's nvid

	using Graphics = std::shared_ptr<VidGraphics>;
	std::variant<std::int32_t, Graphics> graphicsData;

	//
	void read(BinaryStreamReader reader);

	std::ostream& write_csv_line(std::ostream& stream) const {
		auto prettyName = std::string_view{ name.data(),  name.size() };

		stream << prettyName << ',' << +std::to_underlying(unitType) << ',' << +behave << ',' << flags << ','
			<< +collisionMask << ',' << anotherWidth << ',' << anotherHeight << ',' << z_or_height << ','
			<< +maxHP << ',' << gridRadius << ',' << +p6 << ',' << speed << ','
			<< hz1 << ',' << hz2 << ',' << +hz3 << ',' << +army << ',' << +someWeaponIndex << ',' << +hz4 << ',' << deathSizeMargin << ',' << +somethingAboutDeath << ',' << +sX << ',' << +sY << ',' << +sZ << ','
			<< hz5 << ',' << hz6 << ',' << +direction << ',' << +z << ','
			//<< supportedActions << ',' << children << ',' << something << ','
			   << dataSizeOrNvid;

		if (const auto vid = std::get_if<Graphics>(&graphicsData)) {
			stream << ',' << +(*vid)->visualBehavior << ',' << (*vid)->hz7 << ',' << (*vid)->numOfFrames << ',' << (*vid)->dataSize << ',' << (*vid)->imgWidth
				   << ',' << (*vid)->imgHeight;
		}
		else {
			stream << ",-,-,-,-,-,-";
		}

		//std::format("Hello");

		return stream << std::endl;
	}

};

export struct DynamicObject {
	unsigned int nvid;
	int x;
	int y;
	int z;
	int direction;

	struct BasePayload {
		std::uint8_t hp;
};
	struct AdvancedPayload : BasePayload {
		std::optional<std::uint8_t> buildTime;
		std::optional<std::uint8_t> army;
		std::uint8_t behave;
		std::vector<std::int16_t> items;
	};
	using Payload = std::variant<std::monostate, BasePayload, AdvancedPayload>;
	Payload payload;
};

export struct MapHeaderRawData {
	std::uint32_t width;
	std::uint32_t height;
	std::uint16_t observerX;
	std::uint16_t observerY;
	std::uint32_t e;
	std::uint32_t f;
	std::uint32_t startTimer;
	std::uint32_t mapVersion;
};

export class Map
{
public:
	Map() = default;
	explicit Map(std::span<const VidRawData> vids, GromadaResourceReader& reader, GromadaResourceNavigator& resourceNavigator)
		: vids{ vids } {

		loadMapInfo(reader, resourceNavigator);
		loadDynamicObjects(reader, resourceNavigator);
	}

	DynamicObject::Payload readObjectPayload(std::int16_t nvid, BinaryStreamReader& reader, DynamicObject& object) const {
		const auto readStaticObj = [&](DynamicObject::BasePayload& result) {
			result.hp = reader.read<std::uint8_t>();
		};

		const auto readObject2 = [&](DynamicObject::AdvancedPayload& result) {
			readStaticObj(result);

			if (m_header.mapVersion > 2) {
				result.buildTime = reader.read<std::uint8_t>();
			}

			if (m_header.mapVersion > 1) {
				result.army = reader.read<std::uint8_t>();
			}

			result.behave = reader.read<std::uint8_t>();

			if (m_header.mapVersion == 0)
				return;

			for (std::int16_t itemId = 0; itemId = reader.read<std::int16_t>(), itemId >= 0; ) {
				result.items.push_back(itemId);
			}
		};

		static constexpr auto staticClasses = std::to_array<std::uint8_t>({0, 1, 5, 6, 7, 8, 11, 14, 15, 16, 18, 20});
		static constexpr auto dynamicClasses = std::to_array<std::uint8_t>({2, 3, 4, 13, 17});
		static constexpr auto otherClasses = std::to_array<std::uint8_t>({9, 10, 12, 19});


		const auto containsClassPredicate = [id = vids[nvid].behave](std::uint8_t x) { return x == id; };
		if (std::ranges::any_of(staticClasses, containsClassPredicate)) {
			DynamicObject::BasePayload result;
			readStaticObj(result);
			return result;
		}
		else if (std::ranges::any_of(dynamicClasses, containsClassPredicate)) {
			DynamicObject::AdvancedPayload result;
			readObject2(result);
			return result;
	}
		else if (std::ranges::any_of(otherClasses, containsClassPredicate)) {
			return std::monostate{};
		}

		throw std::runtime_error("Unknown object class");
	}

	void loadMapInfo(GromadaResourceReader& reader, GromadaResourceNavigator& resourceNavigator) {
		auto readMapHeader = [&](BinaryStreamReader reader) {
			MapHeaderRawData result;
			reader.read_to(result.width);
			reader.read_to(result.height);
			reader.read_to(result.observerX);
			reader.read_to(result.observerY);
			reader.read_to(result.e);
			reader.read_to(result.f);
			reader.read_to(result.startTimer);
			if (reader.size() < 28) {
				result.mapVersion = 0;
				return result;
			}
			reader.read_to(result.mapVersion);

			return result;
			};

		auto it = std::ranges::find(resourceNavigator.getSections(), SectionType::MapInfo, [](const Section& section) {return section.header().type; });
		if (it == resourceNavigator.getSections().end()) {
			return;
		}

		m_header = readMapHeader(reader.beginRead(*it));
	}

	void loadDynamicObjects(GromadaResourceReader& reader, GromadaResourceNavigator& resourceNavigator) {
		auto readDynamicObjects = [&](BinaryStreamReader reader) {
			std::vector<DynamicObject> result;
			for (auto nvid = 0; nvid = reader.read<std::int16_t>(), nvid > 0; ) {
				std::array<std::uint16_t, 4> rawData;
				reader.read_to(rawData);

				result.push_back({
					.nvid = static_cast<unsigned int>(nvid),
					.x = rawData[0],
					.y = rawData[1],
					.z = rawData[2],
					.direction = rawData[3] //maybe it's direction + action (1 + 1 b)
					});

				readObjectPayload(nvid, reader, result.back());
			}
			return result;
		};


		auto dynamicObjectsSection = resourceNavigator.getSections()
			| std::views::filter([](const Section& section) { return section.header().type == SectionType::Objects; })
			| std::views::transform([&](const Section& section) { return readDynamicObjects(reader.beginRead(section)); })
			| std::views::join
			| std::ranges::to<std::vector>();

		dynamicObjects = std::move(dynamicObjectsSection);
	}

	void write_json(std::ostream& stream) {
		//Json::Value header;
		//header["width"] = m_header.width;
		//header["height"] = m_header.height;
		//header["observerX"] = m_header.observerX;
		//header["observerY"] = m_header.observerY;
		//header["e"] = m_header.e;
		//header["f"] = m_header.f;
		//header["startTimer"] = m_header.startTimer;
		//header["mapVersion"] = m_header.mapVersion;


		//Json::Value objects;
		//for (const auto& obj : dynamicObjects) {
		//	Json::Value object;
		//	object["nvid"] = obj.nvid;
		//	object["x"] = obj.x;
		//	object["y"] = obj.y;
		//	object["z"] = obj.z;
		//	object["direction"] = obj.direction;
		//	object["payload"] = std::visit(overloaded{
		//									   [](const DynamicObject::BasePayload& payload) {
		//										   Json::Value object;
		//										   object["hp"] = payload.hp;
		//										   return object;
		//									   },
		//									   [](const DynamicObject::AdvancedPayload& payload) {
		//										   Json::Value object;
		//										   object["hp"] = payload.hp;
		//										   if (payload.buildTime)
		//											   object["buildTime"] = *payload.buildTime;
		//										   if (payload.army)
		//											   object["army"] = *payload.army;
		//										   object["behave"] = payload.behave;
		//										   for (const auto item : payload.items) {
		//											   object["items"].append(item);
		//										   }
		//										   return object;
		//									   },
		//									   [](const std::monostate&) { return Json::Value{}; },
		//								   },
		//		obj.payload);

		//	objects.append(object);
		//}

		//Json::Value root;
		//root["header"] = header;
		//root["objects"] = objects;

		//stream << root;
	}

	const MapHeaderRawData& header() const noexcept {
		return m_header;
	}

	std::span<const DynamicObject> objects() const noexcept {
		return dynamicObjects;
	}

	std::filesystem::path& filename() noexcept { return m_filename; }
	const std::filesystem::path& filename() const noexcept { return m_filename; }

private:
	std::filesystem::path m_filename;
	std::span<const VidRawData> vids;

	MapHeaderRawData m_header;
	std::vector<DynamicObject> dynamicObjects;
};


void VidRawData::read(BinaryStreamReader reader)
{
	reader.read_to(name);
	reader.read_to(unitType);
	reader.read_to(behave);
	reader.read_to(flags);

	reader.read_to(collisionMask);
	reader.read_to(anotherWidth);
	reader.read_to(anotherHeight);
	reader.read_to(z_or_height);
	reader.read_to(maxHP);
	reader.read_to(gridRadius);
	reader.read_to(p6);
	reader.read_to(speed);

	reader.read_to(hz1);
	reader.read_to(hz2);
	reader.read_to(hz3);
	reader.read_to(army);
	reader.read_to(someWeaponIndex);
	reader.read_to(hz4);
	reader.read_to(deathSizeMargin);
	reader.read_to(somethingAboutDeath);
	reader.read_to(sX);
	reader.read_to(sY);
	reader.read_to(sZ);

	reader.read_to(hz5);
	reader.read_to(hz6);
	reader.read_to(direction);
	reader.read_to(z);

	reader.read_to(supportedActions);
	reader.read_to(children);
	reader.read_to(something);

	reader.read_to(dataSizeOrNvid);

	if (dataSizeOrNvid < 0) {
		return;
	}

	graphicsData = std::make_shared<VidGraphics>();
	std::get<Graphics>(graphicsData)->read(reader);
}


void VidGraphics::read(BinaryStreamReader& reader) {
	reader.read_to(visualBehavior);
	reader.read_to(hz7);
	reader.read_to(numOfFrames);
	reader.read_to(dataSize);
	reader.read_to(imgWidth);
	reader.read_to(imgHeight);

	data.resize(dataSize - sizeof(palette));
	reader.read_to(palette);
	
	frames.resize(numOfFrames);
	reader.read_to(std::span{ data });

	std::byte* frameBegin = data.data();
	for (std::size_t i = 0; i < frames.size(); ++i) {
		//auto offset = *std::start_lifetime_as<std::uint32_t>(frameBegin);
		std::uint32_t offset;
		std::memcpy(&offset, frameBegin, sizeof offset);

		frameBegin += sizeof offset;
		frames[i] = { frameBegin, offset };

		frameBegin += offset;
	}
}

VidGraphics::DecodedData VidGraphics::decode() const {
	std::vector<std::vector<RGBA8>> result;
	result.reserve(frames.size());

	switch (visualBehavior) {
	case 0:
		decodeFormat0(result);
		break;
	case 2:
		decodeFormat2(result);
		break;
	default:
		break;
	}

	return result;
}

void VidGraphics::decodeFormat0(VidGraphics::DecodedData& result) const {
	assert(visualBehavior == 0);

	for (const auto& srcData : frames) {
		if (srcData.size() > 2) {

			std::vector<RGBA8> frame =  srcData.subspan(2) | std::views::transform([this](std::byte color_index) {
				return getPaletteColor(std::to_underlying(color_index));
			}) | std::ranges::to<std::vector<RGBA8>>();

			assert(frame.size() == imgWidth * imgHeight);
			result.push_back(std::move(frame));
		}
		else {
			std::uint16_t frameIndex;
			std::memcpy(&frameIndex, srcData.data(), sizeof frameIndex);
			result.push_back(result[frameIndex]);
		}
	}
}

struct SpanStreamReader {
	SpanStreamReader(std::span<const std::byte> data)
		: data{data} {}
	template <typename T> T read() {
		T result;
		std::memcpy(&result, data.data(), sizeof result);
		data = data.subspan(sizeof result);
		return result;
	}

private:
	std::span<const std::byte> data;
};

void VidGraphics::decodeFormat2(VidGraphics::DecodedData& result) const {
	assert(visualBehavior == 2);

	for (auto srcData : frames) {
		if (srcData.size() > 2) {
			std::vector<RGBA8> frameData(imgWidth * imgHeight);
			std::mdspan frame{frameData.data(), imgHeight, imgWidth};

			SpanStreamReader reader{srcData.subspan(2)};

			const auto startY = reader.read<std::uint16_t>();
			const auto height = reader.read<std::uint16_t>();

			for (std::size_t y = startY; y < startY + height; ++y) {
				std::size_t x = 0;
				while (true) {
					const auto currentByte = reader.read<std::uint8_t>();
					if (currentByte == 0)
						break;
					
					const auto count = currentByte & 0x3F;
					if ((currentByte & 0x80) == 0) {
						if ((currentByte & 0x40) == 0) {
							x += count;
						}
						else {
							for (auto i = 0; i < count; ++i) {
								frame[y, x++] = {0, 0, 0, 128};
							}
						}
					} else {
						if ((currentByte & 0x40) == 0) {
							for (auto i = 0; i < count; ++i) {
								const auto index = reader.read<std::uint8_t>();
								frame[y, x++] = getPaletteColor(index);
							}
						}
						else {
							const auto index = reader.read<std::uint8_t>();
							for (auto i = 0; i < count; ++i) {
								frame[y, x++] = getPaletteColor(index);
							}
						}
					}
				}
			}

			result.push_back(std::move(frameData));
		}
		else {
			std::uint16_t frameIndex;
			std::memcpy(&frameIndex, srcData.data(), sizeof frameIndex);
			result.push_back(result[frameIndex]);
		}
	}
}

export std::vector<StreamSpan> getSounds(GromadaResourceReader& reader, const Section& soundSection) {
	assert(soundSection.header().type == SectionType::Sound);

	std::vector<StreamSpan> result;
	result.reserve(soundSection.header().elementCount);

	BinaryStreamReader soundReader = reader.beginRead(soundSection);
	for (int i = 0; i < soundSection.header().elementCount; ++i) {
		const auto _ = soundReader.read<std::uint8_t>();
		const auto offset = soundReader.read<std::uint32_t>();

		result.emplace_back(soundReader.tellg(), static_cast<std::streamoff>(offset));

		soundReader.skip(offset);
	}

	return result;
}