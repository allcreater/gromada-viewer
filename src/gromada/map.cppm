module;
#include <cstdint>

export module Gromada.Map;

import std;
import utils;
import Gromada.ResourceReader;
import Gromada.Resources;

export struct ObjectCommand {
    Action command;
    std::uint32_t p1, p2;
};

export struct GameObject {
    std::uint16_t nvid;
    std::int16_t x;
    std::int16_t y;
    std::int16_t z;
    std::int16_t direction;

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

export struct Army {
    using Squad = std::vector<std::uint32_t>;
    std::uint32_t a, b, c;
    std::uint32_t flagman_id;
    std::vector<Squad> squads;
};

export struct Map
{
    MapHeaderRawData header;
    std::vector<GameObject> objects;
    std::array<Army, 2> armies;
};

export Map loadMap(std::span<const Vid> vids, const std::filesystem::path& path);
export std::vector<GameObject> loadMenu(std::span<const Vid> vids, std::istream&& stream);
export void saveMap(const Map& map, std::ostream& stream);
