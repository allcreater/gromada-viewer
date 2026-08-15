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

    namespace Payloads {
        struct VisualObject {};
        struct MaterialObject : VisualObject {
            std::uint8_t hp = 0;
        };
        struct AssetObject : MaterialObject {
            std::uint8_t buildTime = 20;
            std::uint8_t army = 0; // Real default is vid[nvid].army
            std::uint8_t behave = 1;
            std::vector<std::int16_t> items;
            std::vector<ObjectCommand> commands;
        };
    }

    struct GameObject {
        std::uint16_t nvid;
        std::int16_t x;
        std::int16_t y;
        std::int16_t z;
        std::uint8_t direction;
        std::uint8_t action = 0; // It seems usually not used in original game

        using Payload = std::variant<Payloads::VisualObject, Payloads::MaterialObject, Payloads::AssetObject>;
        Payload payload;

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
        std::uint32_t scaleX = 24;
        std::uint32_t scaleY = 16;
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

    GameObject::Payload getPayloadPrototype(ObjectClass behavior);
    GameObject::Payload getPayloadPrototype(const Vid& vid);
}

// Implementation
GameObject::Payload getPayloadPrototype(ObjectClass behavior) {
    using enum ObjectClass;
    static constexpr auto materialObjectClasses = std::to_array<ObjectClass>({Terrain, Static, Projectile, CannonMissile, DownedAviaVehicle, NA, Effect1, Shell, AviaMissile, Debris, Bonus, RepairCannon});
    static constexpr auto assetObjectClasses = std::to_array<ObjectClass>({Vehicle, Building, AviaVehicle, Mine, Kassandra});
    static constexpr auto otherClasses = std::to_array<ObjectClass>({Superstructure, Image, Effect2, Font});

    const auto containsClassPredicate = [behavior](ObjectClass x) { return x == behavior; };
    if (std::ranges::any_of(materialObjectClasses, containsClassPredicate))
        return Payloads::MaterialObject{};
    if (std::ranges::any_of(assetObjectClasses, containsClassPredicate))
        return Payloads::AssetObject{};
    if (std::ranges::any_of(otherClasses, containsClassPredicate))
        return Payloads::VisualObject{};

    throw std::runtime_error{"Invalid object class"};
}

GameObject::Payload getPayloadPrototype(const Vid& vid) {
    auto prototype = getPayloadPrototype(vid.type);
    std::visit(overloaded{
        [&](Payloads::AssetObject& payload) {
            payload.army = vid.army;
            payload.hp = vid.maxHP;
        },[&](Payloads::MaterialObject& payload) {
            payload.hp = vid.maxHP;
        },
        [](auto&) {}
    }, prototype);

    return prototype;
}