module;
#include <cstdint>

export module Gromada.Map;

import std;
import Gromada.ResourceReader;
import Gromada.Resources;

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
    MapHeaderRawData header;
    std::vector<GameObject> objects;
};

export Map loadMap(std::span<const Vid> vids, const std::filesystem::path& path);
export std::vector<GameObject> loadMenu(std::span<const Vid> vids, std::istream&& stream);


// Implemenentation

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

Map loadMap(std::span<const Vid> vids, const std::filesystem::path& path) {
	GromadaResourceReader reader{path};
	GromadaResourceNavigator resourceNavigator{reader};

	auto header = loadMapInfo(reader, resourceNavigator);

	return Map{
		.header = header,
		.objects = loadDynamicObjects(header.mapVersion, vids, reader, resourceNavigator),
	};
}

std::vector<GameObject> loadMenu(std::span<const Vid> vids, std::istream&& stream) {
	std::vector<GameObject> result;
	stream.seekg(4, std::ios::beg);
	BinaryStreamReader reader{stream};

	for (int i = 0; i < 16; ++i) {
		readDynamicObjectsSection(result, MapVersion::V0, vids, reader);
	}

	return result;
}