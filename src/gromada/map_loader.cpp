module;
#include <cstdint>

module Gromada.Map;

import std;
import utils;
import Gromada.ResourceReader;


// Implementation
GameObject::Payload readObjectPayload(MapVersion mapVersion, std::uint8_t behavior, BinaryStreamReader& reader) {
    GameObject::Payload result;
    switch(getObjectSerializationClass(behavior)) {
	case ObjectSerializationClass::Static: {
	    result.hp = reader.read<std::uint8_t>();
	} break;
	case ObjectSerializationClass::Dynamic: {
	    result.hp = reader.read<std::uint8_t>();
	    if (std::to_underlying(mapVersion) > 2) {
	        result.buildTime = reader.read<std::uint8_t>();
	    }

	    if (std::to_underlying(mapVersion) > 1) {
	        result.army = reader.read<std::uint8_t>();
	    }

	    result.behave = reader.read<std::uint8_t>();

	    if (std::to_underlying(mapVersion) == 0)
	        break;

	    for (std::int16_t itemId = 0; itemId = reader.read<std::int16_t>(), itemId >= 0;) {
	        result.items.push_back(itemId);
	    }
	} break;
	case ObjectSerializationClass::NoPayload:
        break;
    default:
        throw std::runtime_error("Unknown object class");
    }

    return result;
}

MapHeaderRawData loadMapInfo(GromadaResourceNavigator& resourceNavigator) {
    MapHeaderRawData result;
	auto readMapHeader = [&](const Section& _, BinaryStreamReader reader) {
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

    const auto numSections = resourceNavigator.visitSectionsOfType(SectionType::MapInfo, readMapHeader);
    if (numSections != 1)
        throw std::runtime_error("Invalid map: should be exactly one map info section");

	return result;
}

//TODO: use output iterator
void readDynamicObjectsSection(std::vector<GameObject>& result, MapVersion mapVersion, std::span<const Vid> vids, BinaryStreamReader reader) {
	for (auto nvid = 0; nvid = reader.read<std::int16_t>(), nvid > 0;) {
        if (nvid < 0 || nvid >= vids.size()) [[unlikely]]
	        throw std::runtime_error("Map's object nvid is out of range");

		std::array<std::int16_t, 4> rawData;
		reader.read_to(rawData);

		result.push_back({
			.nvid = static_cast<std::uint16_t>(nvid),
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
        auto& commandArray = lookupSubject(subjectId).payload.commands;

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

std::array<Army, 2> loadArmies(GromadaResourceNavigator& resourceNavigator) {
    std::array<Army, 2> armies;
    std::size_t numOfSections = resourceNavigator.visitSectionsOfType(
        SectionType::Army , [&](const Section& _, BinaryStreamReader reader) {
            if(reader.read<std::uint8_t>() != 2)
                throw std::runtime_error("Invalid map: should be exactly 2 armies");

            for (auto& army : armies) {
                reader.read_to(army.a);
                reader.read_to(army.b);
                reader.read_to(army.c);
                reader.read_to(army.flagman_id);

                for (std::uint32_t id = 0; id = reader.read<std::uint32_t>(), id != 0;) {
                    auto& squad = army.squads.emplace_back();
                    squad.push_back(id);
                    for (std::uint32_t memberId = 0; memberId = reader.read<std::uint32_t>(), memberId != 0;) {
                        squad.push_back(memberId);
                    }
                }
            }
        });

    if (numOfSections != 1) {
        throw std::runtime_error("Invalid map: should be exactly one army section");
    }

    return armies;
}

std::vector<GameObject> loadDynamicObjects(MapVersion mapVersion, std::span<const Vid> vids, GromadaResourceNavigator& resourceNavigator) {
	std::vector<GameObject> result;
	resourceNavigator.visitSectionsOfType(
		SectionType::Objects, [&](const Section& _, BinaryStreamReader reader) { readDynamicObjectsSection(result, mapVersion, vids, reader); });

	std::vector<std::uint32_t> objectIds;
	resourceNavigator.visitSectionsOfType(
		SectionType::ObjectsIds, [&](const Section& _, BinaryStreamReader reader) { readObjectIdsSection(objectIds, reader); });

	if (result.size() != objectIds.size())
		throw std::runtime_error("Map is probably corrupted: ids not matches to objects");

	// linking the objects with their respecive IDs
	for (std::size_t i = 0; i < result.size(); ++i) {
		result[i].id = objectIds[i];
	}

	resourceNavigator.visitSectionsOfType(
		SectionType::Command, [&](const Section& _, BinaryStreamReader reader) { readCommandsSection(objectIds, std::span{result}, reader); });

	return result;
}

Map loadMap(std::span<const Vid> vids, const std::filesystem::path& path) {
	GromadaResourceNavigator resourceNavigator{GromadaResourceReader{path}};

	auto header = loadMapInfo(resourceNavigator);

	return Map{
		.header = header,
		.objects = loadDynamicObjects(header.mapVersion, vids, resourceNavigator),
	    .armies = loadArmies(resourceNavigator),
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