module;
#include <cassert>

export module Gromada.VisualLogic;

import std;
import utils;
import Gromada.Resources;

export enum class Action {
	Stand,
	Build,
	Go,
	StartMove,
	RotateLeft,
	RotateRight,
	Open,
	Close,
	Fight,
	Salut,
	StandOpen,
	Load,
	Unload,
	Wound,
	Birth,
	Death,
};

// TODO: check is it works for different actions
export std::pair<std::size_t, std::size_t> getAnimationFrameRange(const Vid& vid, Action action, std::uint8_t direction) {
	assert(action == Action::Stand);
	auto actionIndex = std::to_underlying(action);
	assert(actionIndex >= 0 && actionIndex < 16);

	if (vid.supportedActions[actionIndex] == 0) {
		actionIndex = 0;
	}

	auto firstFrameIndex = std::accumulate(vid.supportedActions.begin(), vid.supportedActions.begin() + actionIndex, 0,
		[&](int startIndex, std::uint8_t animationLength) { return startIndex + animationLength * vid.directionsCount; });

	const int animationLength = vid.supportedActions[actionIndex];
	const std::uint8_t roundAddition = (0xFF / vid.directionsCount) / 2;
	firstFrameIndex += (((direction + roundAddition) & 0xFF) * vid.directionsCount / 256) * animationLength;

	const auto lastFrameIndex = firstFrameIndex + std::max(animationLength - 1, 0);
	return {firstFrameIndex, lastFrameIndex};
}

export const VidGraphics& getVidGraphics(std::span<const Vid> vids, std::uint16_t nvid) {
	assert(nvid < vids.size());
	return std::visit(overloaded{
						  [vids](std::int32_t referenceVidIndex) -> const VidGraphics& { return getVidGraphics(vids, referenceVidIndex); },
						  [](const Vid::Graphics& graphics) -> const VidGraphics& {
							  if (!graphics) {
								  throw std::runtime_error("vid graphics not loaded");
							  }
							  return *graphics;
						  },
					  },
		vids[nvid].graphicsData);
}