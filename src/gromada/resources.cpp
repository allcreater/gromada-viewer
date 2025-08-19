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

 export enum /*class*/ ObjectFlags : std::uint16_t {
     RandomDirection = 0x1,
     Gravity = 0x2,
     Calltact = 0x4,
     SkipUpdate = 0x8,
     SpawnChildren = 0x10,
     NoCollision = 0x20,
     PresentOnGrid = 0x40,
     Shadow = 0x80,
     Randomized = 0x100,
     Hz1 = 0x200,
     InvisibleSubobjects = 0x400,
     OwnGamma = 0x800,
     Wind = 0x1000,
     Hz2 = 0x2000,
     CollisionBehavior = 0x4000,
};

export std::string to_string(ObjectFlags flags) {
    std::string result;
    if (flags == 0) return "None";

    if (flags & ObjectFlags::RandomDirection) result += "RandomDirection, ";
    if (flags & ObjectFlags::Gravity) result += "Gravity, ";
    if (flags & ObjectFlags::Calltact) result += "Calltact, ";
    if (flags & ObjectFlags::SkipUpdate) result += "SkipUpdate, ";
    if (flags & ObjectFlags::SpawnChildren) result += "SpawnChildren, ";
    if (flags & ObjectFlags::NoCollision) result += "NoCollision, ";
    if (flags & ObjectFlags::PresentOnGrid) result += "PresentOnGrid, ";
    if (flags & ObjectFlags::Shadow) result += "Shadow, ";
    if (flags & ObjectFlags::Randomized) result += "Randomized, ";
    if (flags & ObjectFlags::Hz1) result += "Hz1, ";
    if (flags & ObjectFlags::InvisibleSubobjects) result += "InvisibleSubobjects, ";
    if (flags & ObjectFlags::OwnGamma) result += "OwnGamma, ";
    if (flags & ObjectFlags::Wind) result += "Wind, ";
    if (flags & ObjectFlags::Hz2) result += "Hz2, ";
    if (flags & ObjectFlags::CollisionBehavior) result += "CollisionBehavior, ";

    // Remove trailing comma and space
    if (!result.empty()) {
        result.pop_back();
        result.pop_back();
    }

    return result;
}

export struct VidGraphics {
    VidGraphics() = default;
    explicit VidGraphics(BinaryStreamReader& reader);

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
};

export struct Vid {
    Vid() = default;
    explicit Vid (BinaryStreamReader reader);

	std::array<char, 34> name; // In CP-866
	UnitType unitType;
	std::uint8_t behave;
	ObjectFlags flags;

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

	std::array<std::uint8_t, 16> animationLengths;
    std::array<std::uint16_t, 16> nsfx;
    std::array<std::array<std::int16_t, 16>, 3> childrenOffsets;
	std::array<std::int16_t, 16> childNvid;
	std::array<std::uint8_t, 16> childrenCount;

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
};

Vid::Vid(BinaryStreamReader reader)
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

	reader.read_to(animationLengths);
	reader.read_to(nsfx);
	reader.read_to(childrenOffsets);
	reader.read_to(childNvid);
	reader.read_to(childrenCount);

	reader.read_to(dataSizeOrNvid);

	if (dataSizeOrNvid < 0) {
		graphicsData = std::int32_t{-dataSizeOrNvid};
	} else {
		graphicsData = std::make_shared<VidGraphics>(reader);
	}
}

VidGraphics::VidGraphics(BinaryStreamReader& reader) {
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

	SpanStreamReader frameDataReader{data};
	for (std::size_t i = 0; i < frames.size(); ++i) {
		const auto payloadSize = frameDataReader.read<std::uint32_t>() - 2;
		const auto referenceFrameNumber = frameDataReader.read<std::uint16_t>();

	    if (referenceFrameNumber == 0xFFFF) {
	        frames[i] = {
	            .data = frameDataReader.read_bytes(payloadSize),
                .parent = this,
            };
	    } else {
	        frames[i] = frames.at(referenceFrameNumber);
	    }
	}
}

export std::vector<StreamSpan> getSounds( const Section& soundSection, BinaryStreamReader soundReader) {
	assert(soundSection.header().type == SectionType::Sound);

	std::vector<StreamSpan> result;
	result.reserve(soundSection.header().elementCount);

	for (int i = 0; i < soundSection.header().elementCount; ++i) {
		const auto _ = soundReader.read<std::uint8_t>();
		const auto offset = soundReader.read<std::uint32_t>();

		result.emplace_back(soundReader.tellg(), static_cast<std::streamoff>(offset));

		soundReader.skip(offset);
	}

	return result;
}
