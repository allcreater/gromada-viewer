module;
#include <cassert>

export module Gromada.Terromorphing;

import std;
import utils;
import Gromada.GameResources;
import Gromada.VisualLogic;

export struct TransitionalTileTransition {
    VidRef targetVid;
    bool inversedCoverage;
};

export struct Tile : VidRef {
    std::uint8_t direction = 0;

    auto operator<=>(const Tile&) const = default;
};

export enum class CornerDirection : std::uint8_t {
    TopLeft = 11,
    TopRight = 8,
    BottomRight = 9,
    BottomLeft = 10
};

export std::optional<TransitionalTileTransition> getTilesTransition(VidRef source, VidRef destination ) noexcept;
export std::optional<Tile> trySubstituteTile(Tile sourceTile, VidRef destinationVid, CornerDirection cornerDirection) noexcept;


// Implementation

Tile directionGroupToTile(VidRef vid, int directionGroup) noexcept {
    const auto numVariations = vid->directionsCount / 14;
    const auto direction = numVariations * directionGroup + std::rand() % numVariations;
    return Tile{vid, getDirectionFromIndex(vid->directionsCount, direction)};
}

int tileToDirectionGroup(Tile tile) noexcept {
    return getDirectionIndex(tile->directionsCount, tile.direction) / (tile->directionsCount / 14);
}

std::optional<TransitionalTileTransition> getTilesTransition(VidRef source, VidRef destination ) noexcept {
    if (source == destination)
        return std::nullopt; // No need to do transition

    const GameResources& resources = source ? source.parent() : destination.parent();

    const auto vidToTableIndex = [baseTiles = resources.baseTilesVids()](VidRef baseVid) -> std::optional<int> {
        auto it = std::ranges::find(baseTiles, baseVid);
        if (it == baseTiles.end())
            return std::nullopt;

        return static_cast<int>(std::distance(baseTiles.begin(), it));
    };

    const auto srcIndex = vidToTableIndex(source);
    const auto dstIndex = vidToTableIndex(destination);

    if (!dstIndex)
        return std::nullopt; // At least not implemented

    if (srcIndex) { // It means that both source and destination tiles was bases
        const auto nvid = resources.adjacencyData()()[*dstIndex, *srcIndex];
        if (nvid == 0) // Ok, no transition
            return std::nullopt;

        return TransitionalTileTransition{resources.getVid( std::abs(nvid)), nvid < 0};
    }

    // Else: it's already composite, but we still need to determine coverage direction
    const int tableStride = resources.adjacencyData()().extent( 1 );
    const auto tableRow = std::span{resources.adjacencyData().data}.subspan(*dstIndex * tableStride, tableStride);
    const auto it = std::ranges::find(tableRow, source.nvid(), [](auto x){return std::abs(x);});
    if (it == tableRow.end() || !source)
        return std::nullopt;

    return TransitionalTileTransition{source, *it < 0};
}

constexpr std::uint8_t flipCoverage(std::uint8_t coverageMask, bool flip) noexcept {
    return coverageMask ^ (flip ? 0x0F : 0x0);
}

std::optional<Tile> trySubstituteTile(Tile sourceTile, VidRef destinationVid, CornerDirection cornerDirection) noexcept {
    assert(sourceTile);

    if (sourceTile == destinationVid)
        return sourceTile;

    constexpr static std::array<std::uint8_t, 14> directionIndexToCoverageMask{
        0b1100, 0b1110, 0b0110, 0b0111,
        0b0011, 0b1011, 0b1001, 0b1101,
        0b0001, 0b1000, 0b0100, 0b0010,
        0b1010, 0b0101
    };
    // mask always accumulates in "destinationVid ownership" sense: bit set means that part of the tile belongs to destinationVid's type
    auto mask = directionIndexToCoverageMask[std::to_underlying(cornerDirection)];

    const auto transition = getTilesTransition(sourceTile, destinationVid);
    if (!transition) {
        return std::nullopt;
    }

    const auto [resultVid, invertedCoverage] = *transition;
    if (resultVid == sourceTile) {
        mask |= flipCoverage(directionIndexToCoverageMask[tileToDirectionGroup(sourceTile)], invertedCoverage);
    }

    if (mask == 0x0F)
        return Tile{destinationVid,  static_cast<std::uint8_t>(destinationVid ?  std::rand() % destinationVid->directionsCount : 0)};

    const auto maskIt = std::ranges::find(directionIndexToCoverageMask, flipCoverage(mask, invertedCoverage));
    if (maskIt == directionIndexToCoverageMask.end() || !resultVid)
        return std::nullopt;

    return directionGroupToTile(resultVid, std::distance(directionIndexToCoverageMask.begin(), maskIt));
}
