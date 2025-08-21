module;
#include <cstdint>

export module Gromada.Map;

import std;
import utils;
import Gromada.ResourceReader;
import Gromada.Resources;

export {
    struct ObjectCommand {
        Action command;
        std::uint32_t p1, p2;
    };

    struct GameObject {
        std::uint16_t nvid;
        std::int16_t x;
        std::int16_t y;
        std::int16_t z;
        std::uint8_t direction;
        std::uint8_t action = 0; // It seems usually not used in original game

        // NOTE: this is all fields that are loaded in the original game
        // Not all of them may be saved/loaded at same time: it depends on the object type (specifically, vid[nvid].behave)
        struct Payload {
            std::vector<ObjectCommand> commands;

            // for most static objects
            std::uint8_t hp = 0;
            // For units
            std::uint8_t buildTime = 20;
            std::uint8_t army = 0; // Real default is vid[nvid].army
            std::uint8_t behave = 1;
            std::vector<std::int16_t> items;
        } payload;

        std::uint32_t id; // Unique ID for the object, used as a target for some commands and map armies info
    };

    enum /*class*/ MapVersion : std::uint32_t {
        V0 = 0,
        V1 = 1,
        V2 = 2,
        V3 = 3,
    };

    struct MapHeaderRawData {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::int16_t observerX = 0;
        std::int16_t observerY = 0;
        std::uint32_t e = 0;
        std::uint32_t f = 0;
        std::uint32_t startTimer = 0;
        MapVersion mapVersion = MapVersion::V3;
    };

    struct Army {
        using Squad = std::vector<std::uint32_t>;
        std::uint32_t a, b, c;
        std::uint32_t flagman_id;
        std::vector<Squad> squads;
    };

    struct Map
    {
        MapHeaderRawData header;
        std::vector<GameObject> objects;
        std::array<Army, 2> armies;
    };

    Map loadMap(std::span<const Vid> vids, const std::filesystem::path& path);
    std::vector<GameObject> loadMenu(std::span<const Vid> vids, std::istream&& stream);
    void saveMap(std::span<const Vid> vids, const Map& map, std::ostream& stream);

    enum class ObjectSerializationClass : std::uint8_t {
        Unknown,
        NoPayload,
        Static,
        Dynamic,
    };
    ObjectSerializationClass getObjectSerializationClass(std::uint8_t behavior) noexcept;
}

// Implementation
ObjectSerializationClass getObjectSerializationClass(std::uint8_t behavior) noexcept {
    static constexpr auto staticClasses = std::to_array<std::uint8_t>({0, 1, 5, 6, 7, 8, 11, 14, 15, 16, 18, 20});
    static constexpr auto dynamicClasses = std::to_array<std::uint8_t>({2, 3, 4, 13, 17});
    static constexpr auto otherClasses = std::to_array<std::uint8_t>({9, 10, 12, 19});

    const auto containsClassPredicate = [behavior](std::uint8_t x) { return x == behavior; };
    if (std::ranges::any_of(staticClasses, containsClassPredicate))
        return ObjectSerializationClass::Static;
    if (std::ranges::any_of(dynamicClasses, containsClassPredicate))
        return ObjectSerializationClass::Dynamic;
    if (std::ranges::any_of(otherClasses, containsClassPredicate))
        return ObjectSerializationClass::NoPayload;

    return ObjectSerializationClass::Unknown;
}