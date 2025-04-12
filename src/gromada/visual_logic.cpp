module;
#include <cassert>

export module Gromada.VisualLogic;

import std;
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

	const auto animationLength = vid.supportedActions[actionIndex];
	const std::uint8_t roundAddition = (0xFF / vid.directionsCount) / 2;
	firstFrameIndex += (((direction + roundAddition) & 0xFF) * vid.directionsCount / 256) * animationLength;

	const auto lastFrameIndex = firstFrameIndex + animationLength - 1;
	return {firstFrameIndex, lastFrameIndex};
}