module;
#include <cassert>

export module Gromada.VisualLogic;

import std;
import utils;
import Gromada.Actions;
import Gromada.Resources;

export class Stopwatch {
public:
	Stopwatch(float tick_period_seconds = {}) : next_frame_delay{tick_period_seconds} {}
	[[nodiscard]] int advance(float dt, float tick_period_seconds) noexcept {
		next_frame_delay -= dt;
		if (next_frame_delay < 0.0f) {
			int increment = std::ceil(-next_frame_delay / tick_period_seconds);
			next_frame_delay += increment * tick_period_seconds;

			return increment;
		}

		return 0;
	}

private:
	float next_frame_delay;
};

export int getDirectionIndex(std::uint8_t directionsCount, std::uint8_t direction) {
	const std::uint8_t roundAddition = (256 / directionsCount) / 2;
	return ((direction + roundAddition) & 0xFF) * directionsCount / 256;
}

export std::optional<std::pair<std::size_t, std::size_t>> getAnimationFrameRangeDirIndex(const Vid& vid, Action action, int directionIndex) {
	auto actionIndex = std::to_underlying(action);
	assert(actionIndex >= 0 && actionIndex < 16);
	assert(directionIndex >= 0 && directionIndex < vid.directionsCount);

	if (vid.animationLengths[actionIndex] == 0) {
		return std::nullopt;
	}

	const int animationLength = vid.animationLengths[actionIndex];
	auto firstFrameIndex = std::accumulate(vid.animationLengths.begin(), vid.animationLengths.begin() + actionIndex, 0) * vid.directionsCount + directionIndex * animationLength;

	const auto lastFrameIndex = firstFrameIndex + std::max(animationLength - 1, 0);
	return std::pair{firstFrameIndex, lastFrameIndex};
}

export std::optional<std::pair<std::size_t, std::size_t>> getAnimationFrameRange(const Vid& vid, Action action, std::uint8_t direction) {
	const auto directionIndex = getDirectionIndex(vid.directionsCount, direction);
	return getAnimationFrameRangeDirIndex(vid, action, directionIndex).or_else( [&] { return getAnimationFrameRangeDirIndex(vid, Action::act_stand, directionIndex); });
}