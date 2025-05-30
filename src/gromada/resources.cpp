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

export module Gromada.Resources;

import Gromada.ResourceReader;
import std;
import utils;
import nlohmann_json_adapter;

export enum class UnitType : std::uint8_t {
	Terrain = 0x1,
	Object = 0x2,
	Monster = 0x4,
	Avia = 0x8,
	Cannon = 0x10,
	Sprite = 0x20,
	Item = 0x40,
};

//TODO: move to software_renderer
export struct RGBA8 {
	std::uint8_t r;
	std::uint8_t g;
	std::uint8_t b;
	std::uint8_t a;
};

//export enum class ObjectFlags : std::uint16_t {
//	Shadow = 0x80,
//};

export struct VidGraphics {
	std::uint8_t dataFormat;
	std::uint16_t hz7;
	std::uint16_t numOfFrames;
	std::uint32_t dataSize;
	std::uint16_t imgWidth;
	std::uint16_t imgHeight;

	std::array<std::uint8_t, 0x300> palette;
	std::vector<std::byte> data;

	struct Frame {
		std::uint16_t referenceFrameNumber;
		std::span<std::byte> data;
	};
	std::vector<Frame> frames;

	// TODO: move out of there
	RGBA8 getPaletteColor(std::uint8_t index) const {
		auto offset = index * 3;
		return {palette[offset], palette[offset + 1], palette[offset + 2], 255};
	}

	// TODO: add validation of the input data because it may cause out of range access 
	void read(BinaryStreamReader& reader);

	using DecodedData = std::vector<std::vector<RGBA8>>;
	DecodedData decode() const;

private:
	void decodeFormat0(DecodedData& out) const;
	void decodeFormat2(DecodedData& out) const;
};

export struct Vid {
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

	std::uint16_t speedX;
	std::uint16_t speedY;
	std::uint16_t acceleration;
	std::uint8_t rotationPeriod;

	std::uint8_t army;
	std::uint8_t someWeaponIndex;
	std::uint8_t hz4;
	std::uint16_t deathDamageRadius;
	std::uint8_t deathDamage;

	std::uint8_t linkX;
	std::uint8_t linkY;
	std::uint8_t linkZ;
	std::uint16_t linkedObjectVid;

	std::uint16_t hz6;
	std::uint8_t directionsCount;
	std::uint8_t z; // z_priority?

	// TODO: std::array<std::uint8_t, 16> animationLengths; ?
	std::array<std::uint8_t, 144> supportedActions;
	std::array<std::uint16_t, 16> children;
	std::array<std::uint8_t, 16> something;

	std::int32_t dataSizeOrNvid; // if < 0 then it's nvid

	using Graphics = std::shared_ptr<VidGraphics>;
	std::variant<std::int32_t, Graphics> graphicsData;

	//
    const VidGraphics& graphics() const {
        const auto* graphics = std::get_if<Vid::Graphics>(&graphicsData);
        if (!graphics || !graphics->get())
            throw std::logic_error("Missing graphics");

        return **graphics;
    }
	void read(BinaryStreamReader reader);
};

export struct GameObject {
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

export enum /*class*/ MapVersion : std::uint32_t {
	V0 = 0,
	V1 = 1,
	V2 = 2,
	V3 = 3,
};

export struct MapHeaderRawData {
	std::uint32_t width;
	std::uint32_t height;
	std::int16_t observerX;
	std::int16_t observerY;
	std::uint32_t e;
	std::uint32_t f;
	std::uint32_t startTimer;
	MapVersion mapVersion;
};


export struct Map
{
	static Map load(std::span<const Vid> vids, const std::filesystem::path& path);

	MapHeaderRawData header;
	std::vector<GameObject> objects;
};


void Vid::read(BinaryStreamReader reader)
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

	reader.read_to(speedX);
	reader.read_to(speedY);
	reader.read_to(acceleration);
	reader.read_to(rotationPeriod);

	reader.read_to(army);
	reader.read_to(someWeaponIndex);
	reader.read_to(hz4);
	reader.read_to(deathDamageRadius);
	reader.read_to(deathDamage);

	reader.read_to(linkX);
	reader.read_to(linkY);
	reader.read_to(linkZ);
	reader.read_to(linkedObjectVid);

	reader.read_to(hz6);
	reader.read_to(directionsCount);
	reader.read_to(z);

	reader.read_to(supportedActions);
	reader.read_to(children);
	reader.read_to(something);

	reader.read_to(dataSizeOrNvid);

	if (dataSizeOrNvid < 0) {
		graphicsData = std::int32_t{-dataSizeOrNvid};
	}
	else {
		graphicsData = std::make_shared<VidGraphics>();
		std::get<Graphics>(graphicsData)->read(reader);
	}
}


void VidGraphics::read(BinaryStreamReader& reader) {
	reader.read_to(dataFormat);
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
		//auto payloadSize = *std::start_lifetime_as<std::uint32_t>(frameBegin);
		std::uint32_t payloadSize;
		std::memcpy(&payloadSize, frameBegin, sizeof payloadSize);
		frameBegin += sizeof payloadSize;

		std::uint16_t referenceFrameNumber;
		std::memcpy(&referenceFrameNumber, frameBegin, sizeof referenceFrameNumber);
		frameBegin += sizeof referenceFrameNumber;
		payloadSize -= 2;

		frames[i] = {
			.referenceFrameNumber = referenceFrameNumber,
			.data = {frameBegin, payloadSize},
		};

		frameBegin += payloadSize;
	}
}

VidGraphics::DecodedData VidGraphics::decode() const {
	std::vector<std::vector<RGBA8>> result;
	result.reserve(frames.size());

	switch (dataFormat) {
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
	assert(dataFormat == 0);

	for (const auto& [referenceFrameNumber, srcData] : frames) {
		if (srcData.size() > 0) {

			std::vector<RGBA8> frame =  srcData| std::views::transform([this](std::byte color_index) {
				return getPaletteColor(std::to_underlying(color_index));
			}) | std::ranges::to<std::vector<RGBA8>>();

			assert(frame.size() == imgWidth * imgHeight);
			result.push_back(std::move(frame));
		}
		else {
			result.push_back(result[referenceFrameNumber]);
		}
	}
}


void VidGraphics::decodeFormat2(VidGraphics::DecodedData& result) const {
	assert(dataFormat == 2);

	for (auto [referenceFrameNumber, srcData] : frames) {
		if (srcData.size() > 0) {
			std::vector<RGBA8> frameData(imgWidth * imgHeight);
			std::mdspan frame{frameData.data(), imgHeight, imgWidth};

			SpanStreamReader reader{srcData};

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
			result.push_back(result[referenceFrameNumber]);
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


// Map loading

GameObject::Payload readObjectPayload(MapVersion mapVersion, std::uint8_t behavior, BinaryStreamReader& reader) {
	const auto readStaticObj = [&](GameObject::BasePayload& result) { result.hp = reader.read<std::uint8_t>(); };

	const auto readObject2 = [&](GameObject::AdvancedPayload& result) {
		readStaticObj(result);

		if (std::to_underlying(mapVersion) > 2) {
			result.buildTime = reader.read<std::uint8_t>();
		}

		if (std::to_underlying(mapVersion) > 1) {
			result.army = reader.read<std::uint8_t>();
		}

		result.behave = reader.read<std::uint8_t>();

		if (std::to_underlying(mapVersion) == 0)
			return;

		for (std::int16_t itemId = 0; itemId = reader.read<std::int16_t>(), itemId >= 0;) {
			result.items.push_back(itemId);
		}
	};

	static constexpr auto staticClasses = std::to_array<std::uint8_t>({0, 1, 5, 6, 7, 8, 11, 14, 15, 16, 18, 20});
	static constexpr auto dynamicClasses = std::to_array<std::uint8_t>({2, 3, 4, 13, 17});
	static constexpr auto otherClasses = std::to_array<std::uint8_t>({9, 10, 12, 19});


	const auto containsClassPredicate = [behavior](std::uint8_t x) { return x == behavior; };
	if (std::ranges::any_of(staticClasses, containsClassPredicate)) {
		GameObject::BasePayload result;
		readStaticObj(result);
		return result;
	}
	else if (std::ranges::any_of(dynamicClasses, containsClassPredicate)) {
		GameObject::AdvancedPayload result;
		readObject2(result);
		return result;
	}
	else if (std::ranges::any_of(otherClasses, containsClassPredicate)) {
		return std::monostate{};
	}

	throw std::runtime_error("Unknown object class");
}

MapHeaderRawData loadMapInfo(GromadaResourceReader& reader, GromadaResourceNavigator& resourceNavigator) {
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
			result.mapVersion = MapVersion::V0;
			return result;
		}
		reader.read_to(result.mapVersion);

		return result;
	};

	auto it = std::ranges::find(resourceNavigator.getSections(), SectionType::MapInfo, [](const Section& section) { return section.header().type; });
	if (it == resourceNavigator.getSections().end()) {
		throw std::runtime_error("Invalid map: no header section");
	}

	return readMapHeader(reader.beginRead(*it));
}

//TODO: use output iterator
void readDynamicObjectsSection(std::vector<GameObject>& result, MapVersion mapVersion, std::span<const Vid> vids, BinaryStreamReader reader) {
	for (auto nvid = 0; nvid = reader.read<std::int16_t>(), nvid > 0;) {
		std::array<std::uint16_t, 4> rawData;
		reader.read_to(rawData);

		result.push_back({
			.nvid = static_cast<unsigned int>(nvid),
			.x = rawData[0],
			.y = rawData[1],
			.z = rawData[2],
			.direction = rawData[3], // maybe it's direction + action (1 + 1 b)
			.payload = readObjectPayload(mapVersion, vids[nvid].behave, reader),
		});
	}
}

std::vector<GameObject> loadDynamicObjects(
	MapVersion mapVersion, std::span<const Vid> vids, GromadaResourceReader& reader, GromadaResourceNavigator& resourceNavigator) {
	std::vector<GameObject> result;
	
	std::ranges::for_each(
		resourceNavigator.getSections() | std::views::filter([](const Section& section) { return section.header().type == SectionType::Objects; }),
		[&](const Section& section) { return readDynamicObjectsSection(result, mapVersion, vids, reader.beginRead(section)); }

	);

	return result;
}

Map Map::load(std::span<const Vid> vids, const std::filesystem::path& path) {
	GromadaResourceReader reader{path};
	GromadaResourceNavigator resourceNavigator{reader};

	auto header = loadMapInfo(reader, resourceNavigator);

	return Map{
		.header = header,
		.objects = loadDynamicObjects(header.mapVersion, vids, reader, resourceNavigator),
	};
}

export std::vector<GameObject> loadMenu(std::span<const Vid> vids, std::istream&& stream) {
	std::vector<GameObject> result;
	stream.seekg(4, std::ios::beg);
	BinaryStreamReader reader{stream};

	for (int i = 0; i < 16; ++i) {
		readDynamicObjectsSection(result, MapVersion::V0, vids, reader);
	}

	return result;
}