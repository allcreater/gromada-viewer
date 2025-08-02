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
#include <cstdint>

export module Gromada.Resources;

export import Gromada.Actions;
import Gromada.ResourceReader;
import std;
import utils;
import cp866;
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

export constexpr std::string_view to_string(UnitType unitType) {
    using enum UnitType;
    switch (unitType) {
    case Terrain: return "Terrain";
    case Object: return "Object";
    case Monster: return "Monster";
    case Avia: return "Avia";
    case Cannon: return "Cannon";
    case Sprite: return "Sprite";
    case Item: return "Item";
    default:
        return "Unknown";
    }
}


export struct ColorRgb8 {
	std::uint8_t r;
	std::uint8_t g;
	std::uint8_t b;
};

// export enum class ObjectFlags : std::uint16_t {
//	Shadow = 0x80,
//};

export struct VidGraphics {
	std::uint8_t dataFormat;
	std::uint16_t frameDuration;
	std::uint16_t numOfFrames;
	std::uint32_t dataSize;
	std::uint16_t width;
	std::uint16_t height;

	std::array<ColorRgb8, 256> palette;
	std::vector<std::byte> data;

	struct Frame {
		std::span<const std::byte> data;
	    const VidGraphics* parent;

	    SpanStreamReader read() const noexcept {return data; }
	    [[nodiscard]] int width() const noexcept { return parent->width; }
	    [[nodiscard]] int height() const noexcept { return parent->height; }
	};
	std::vector<Frame> frames;

	// TODO: add validation of the input data because it may cause out of range access 
	void read(BinaryStreamReader& reader);
};

export struct Vid {
	std::array<char, 34> name; // In CP-866
	UnitType unitType;
	std::uint8_t behave;
	std::uint16_t flags;

	std::uint8_t collisionMask;
	std::uint16_t sizeX;
	std::uint16_t sizeY;
	std::uint16_t sizeZ;
	std::uint8_t maxHP;
	std::uint16_t gridRadius;
	std::uint8_t unused1;

	std::uint16_t speedX;
	std::uint16_t speedY;
	std::uint16_t acceleration;
	std::uint8_t rotationPeriod;

	std::uint8_t army;
	std::uint8_t someWeaponIndex;
	std::uint8_t unused2;
	std::uint16_t deathDamageRadius;
	std::uint8_t deathDamage;

	std::int8_t linkX;
	std::int8_t linkY;
	std::int8_t linkZ;
	std::uint16_t linkedObjectVid;

	std::uint16_t unused3;
	std::uint8_t directionsCount;
	std::uint8_t z_layer;

	// TODO: std::array<std::uint8_t, 16> animationLengths; ?
	std::array<std::uint8_t, 144> supportedActions;
	std::array<std::uint16_t, 16> children;
	std::array<std::uint8_t, 16> something;

	std::int32_t dataSizeOrNvid; // if < 0 then it's nvid

	using Graphics = std::shared_ptr<VidGraphics>;
	std::variant<std::int32_t, Graphics> graphicsData;

	//
	[[nodiscard]] std::u8string getName() const { return cp866_to_utf8(std::string_view{name.data()}); }
    const VidGraphics& graphics() const {
        const auto* graphics = std::get_if<Vid::Graphics>(&graphicsData);
        if (!graphics || !graphics->get())
            throw std::logic_error("Missing graphics");

        return **graphics;
    }
	void read(BinaryStreamReader reader);
};

export struct ObjectCommand {
    Action command;
    std::uint32_t p1, p2;
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

    std::uint32_t id; // Unique ID for the object, used for commands
    std::vector<ObjectCommand> commands;
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
	reader.read_to(sizeX);
	reader.read_to(sizeY);
	reader.read_to(sizeZ);
	reader.read_to(maxHP);
	reader.read_to(gridRadius);
	reader.read_to(unused1);

	reader.read_to(speedX);
	reader.read_to(speedY);
	reader.read_to(acceleration);
	reader.read_to(rotationPeriod);

	reader.read_to(army);
	reader.read_to(someWeaponIndex);
	reader.read_to(unused2);
	reader.read_to(deathDamageRadius);
	reader.read_to(deathDamage);

	reader.read_to(linkX);
	reader.read_to(linkY);
	reader.read_to(linkZ);
	reader.read_to(linkedObjectVid);

	reader.read_to(unused3);
	reader.read_to(directionsCount);
	reader.read_to(z_layer);

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
	reader.read_to(frameDuration);
	reader.read_to(numOfFrames);
	reader.read_to(dataSize);
	reader.read_to(width);
	reader.read_to(height);

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

	    if (referenceFrameNumber == 0xFFFF) {
	        frames[i] = {
	            .data = {frameBegin, payloadSize},
                .parent = this,
            };
	    } else {
	        assert(referenceFrameNumber < i);
	        frames[i] = frames.at(referenceFrameNumber);
	    }

		frameBegin += payloadSize;
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

void readObjectIdsSection(std::vector<std::uint32_t>& objectIds, BinaryStreamReader reader) {
    const auto count = reader.read<std::uint32_t>();

    objectIds.resize(objectIds.size() + count);
    reader.read_to(std::as_writable_bytes(std::span{objectIds}.subspan(objectIds.size() - count)));
}

void readCommandsSection(std::span<const std::uint32_t> objectIds, std::span<GameObject> objects, BinaryStreamReader reader) {
    //const std::map<std::uint32_t, std::size_t>& objectsLookup,
    auto lookupSubject = [&](std::uint32_t subjectId) -> GameObject& {
        const auto it = std::ranges::find(objectIds, subjectId);
        if (it == objectIds.end()) [[unlikely]]
            throw std::out_of_range("Invalid object id");

        return objects[std::distance(objectIds.begin(), it)];
    };

    for (std::uint32_t subjectId; subjectId = reader.read<std::uint32_t>(); ) {
        auto& commandArray = lookupSubject(subjectId).commands;

        const auto count = reader.read<std::int32_t>();
        commandArray.reserve(count);
        for (int i = 0; i < count; i++) {
            commandArray.push_back(ObjectCommand {
                .command = Action{reader.read<std::uint8_t>()},
                .p1 =  reader.read<std::uint32_t>(),
                .p2 =  reader.read<std::uint32_t>(),
            });
        }
    }
}

std::vector<GameObject> loadDynamicObjects(
	MapVersion mapVersion, std::span<const Vid> vids, GromadaResourceReader& reader, GromadaResourceNavigator& resourceNavigator) {
    auto visitSectionsOfType = [&](SectionType sectionType, std::invocable<BinaryStreamReader> auto&& visitor) {
        std::ranges::for_each(
            resourceNavigator.getSections() | std::views::filter([&](const Section& section) { return section.header().type == sectionType; }),
            [&](const Section& section) { visitor(reader.beginRead(section)); });
    };

    std::vector<GameObject> result;
    visitSectionsOfType(SectionType::Objects, std::bind_front(readDynamicObjectsSection, std::ref(result), mapVersion, vids));

    std::vector<std::uint32_t> objectIds;
    visitSectionsOfType(SectionType::ObjectsIds, std::bind_front(readObjectIdsSection, std::ref(objectIds)));

    if (result.size() != objectIds.size())
        throw std::runtime_error("Map is probably corrupted: ids not matches to objects");

    // linking the objects with their respecive IDs
	for (std::size_t i = 0; i < result.size(); ++i) {
	    result[i].id = objectIds[i];
	}

    visitSectionsOfType(SectionType::Command, std::bind_front(readCommandsSection, std::span{objectIds}, std::span{result}));

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