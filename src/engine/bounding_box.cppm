export module engine.bounding_box;

import std;

// TODO: use full-featured 2D AABB's instead
export struct BoundingBox {
	int left;
	int right;
	int top;
	int down;

	constexpr bool empty() const noexcept { return (right - left <= 0) || (down - top <= 0); }

	constexpr BoundingBox intersection(const BoundingBox& other) const noexcept {
		return {
			.left = std::max(left, other.left),
			.right = std::min(right, other.right),
			.top = std::max(top, other.top),
			.down = std::min(down, other.down),
		};
	}

	constexpr bool isIntersects(const BoundingBox& other) const noexcept { return !intersection(other).empty(); }
};
