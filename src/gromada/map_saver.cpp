module;
#include <cstdint>

module Gromada.Map;

import std;
import utils;
import Gromada.ResourceReader;


// Implementation
class SectionWriter {
private:
    std::ostream& stream;
    std::streampos sectionStartPos;
    SectionHeader sectionHeader{};

public:
    SectionWriter(SectionType type, std::uint32_t elementCount, std::ostream& stream)
        : stream{stream}, sectionStartPos{stream.tellp()}, sectionHeader{.type = type, .elementCount = elementCount, .dataOffset = 0} {
        write(sectionHeader.type);
        write(sectionHeader.nextSectionOffset);
        write(sectionHeader.elementCount);
        write(sectionHeader.dataOffset);
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    void write(const T& value) {
        stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    ~SectionWriter() {
        auto currentPos = stream.tellp();
        stream.seekp(sectionStartPos + std::ios::off_type{1}, std::ios::beg);
        write<std::uint32_t>(currentPos - sectionStartPos - 5);
        stream.seekp(currentPos, std::ios::beg);
    }

};

void saveMap(std::span<const Vid> vids, const Map& map, std::ostream& stream) {
    const std::uint32_t sectionCount = 5; // MapInfo, Objects, ObjectsIds, Command, Army
    stream.write(reinterpret_cast<const char*>(&sectionCount), sizeof(sectionCount));

    {
        SectionWriter writer{SectionType::MapInfo, 1, stream};
        MapHeaderRawData mapHeaderCopy {map.header};
        mapHeaderCopy.mapVersion = MapVersion::V3; // Ensure the map version is set to the latest
        writer.write(mapHeaderCopy);
    }

    {
        SectionWriter writer{SectionType::Objects, 1, stream};
        for (const auto& obj : map.objects) {
            writer.write(obj.nvid);
            writer.write(obj.x);
            writer.write(obj.y);
            writer.write(obj.z);
            writer.write(obj.direction);
            writer.write(obj.action);

            switch(getObjectSerializationClass(vids[obj.nvid].behave)) {
            case ObjectSerializationClass::Static: {
                    writer.write(obj.payload.hp);
                } break;
            case ObjectSerializationClass::Dynamic: {
                    writer.write(obj.payload.hp);
                    writer.write(obj.payload.buildTime);
                    writer.write(obj.payload.army);
                    writer.write(obj.payload.behave);
                    for (const auto itemId : obj.payload.items) {
                        writer.write(itemId);
                    }
                    writer.write(static_cast<std::int16_t>(-1)); // Items terminator
                } break;
            }
        }
        writer.write<std::uint16_t>(-1); // Objects terminator
    }

    {
        SectionWriter writer{SectionType::ObjectsIds, 1, stream};
        writer.write(static_cast<std::uint32_t>(map.objects.size()));
        for (const auto& obj : map.objects) {
            writer.write(obj.id);
        }
    }

    {
        SectionWriter writer{SectionType::Command, 1, stream};

        auto objects = map.objects | std::views::filter([](const GameObject& obj) { return !obj.payload.commands.empty(); });
        for (const auto& obj : objects) {
            writer.write(obj.id);
            writer.write(static_cast<std::int32_t>(obj.payload.commands.size()));
            for (const auto& command : obj.payload.commands) {
                writer.write(static_cast<std::uint8_t>(command.command));
                writer.write(command.p1);
                writer.write(command.p2);
            }
        }
        writer.write<std::uint32_t>(0); // Commands terminator

    }

    {
        SectionWriter writer{SectionType::Army, 1, stream};
        writer.write<std::uint8_t>(2); // Number of armies
        for (const auto& army : map.armies) {
            writer.write(army.a);
            writer.write(army.b);
            writer.write(army.c);
            writer.write(army.flagman_id);
            for (const auto& squad : army.squads) {
                for (const auto memberId : squad) {
                    writer.write(memberId);
                }
                writer.write<std::uint32_t>(0); // Members terminator
            }
            writer.write<std::uint32_t>(0); // Squads terminator
        }
    }

}