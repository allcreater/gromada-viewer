module;
#include <cassert>

export module Gromada.VisualLogic;

import std;
import utils;
import Gromada.Actions;
import Gromada.Resources;

// TODO: check is it works for different actions
export std::pair<std::size_t, std::size_t> getAnimationFrameRange(const Vid& vid, Action action, std::uint8_t direction) {
	assert(action == Action::act_stand);
	auto actionIndex = std::to_underlying(action);
	assert(actionIndex >= 0 && actionIndex < 16);

	if (vid.animationLengths[actionIndex] == 0) {
		actionIndex = 0;
	}

	auto firstFrameIndex = std::accumulate(vid.animationLengths.begin(), vid.animationLengths.begin() + actionIndex, 0,
		[&](int startIndex, std::uint8_t animationLength) { return startIndex + animationLength * vid.directionsCount; });

	const int animationLength = vid.animationLengths[actionIndex];
	const std::uint8_t roundAddition = (256 / vid.directionsCount) / 2;
	firstFrameIndex += (((direction + roundAddition) & 0xFF) * vid.directionsCount / 256) * animationLength;

	const auto lastFrameIndex = firstFrameIndex + std::max(animationLength - 1, 0);
	return {firstFrameIndex, lastFrameIndex};
}
