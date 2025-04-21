export module engine.bounding_box;

import std;

template <typename T, typename ComponentType>
concept vector2 = requires(T vector) {
	{ vector.x } -> std::convertible_to<ComponentType>;
	{ vector.y } -> std::convertible_to<ComponentType>;
	{ vector + vector } -> std::same_as<T>;
};

// TODO: use full-featured 2D AABB's instead
export struct BoundingBox {
	int left;
	int right;
	int top;
	int down;

	template <vector2<int> Vector>
	[[nodiscard]] static constexpr BoundingBox fromPositions(const Vector& a, const Vector& b) noexcept { 
		return {
			.left = std::min(a.x, b.x),
			.right = std::max(a.x, b.x),
			.top = std::min(a.y, b.y),
			.down = std::max(a.y, b.y),
		};
	}

	template <vector2<int> Vector> 
	[[nodiscard]] static constexpr BoundingBox fromPositionAndSize(const Vector& topLeft, const Vector& size) { 
		return fromPositions(topLeft, topLeft + size);
	}

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
