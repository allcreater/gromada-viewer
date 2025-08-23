export module engine.bounding_box;

import std;

// TODO: use full-featured 2D AABB's instead
export struct BoundingBox {
	int left;
	int right;
	int top;
	int down;

	[[nodiscard]] static constexpr BoundingBox fromPositions(int x1, int y1, int x2, int y2) noexcept { 
		return {
			.left = std::min(x1, x2),
			.right = std::max(x1, x2),
			.top = std::min(y1, y2),
			.down = std::max(y1, y2),
		};
	}

	[[nodiscard]] static constexpr BoundingBox fromPositionAndSize(int x, int y, int sizeX, int sizeY) noexcept { 
		return fromPositions(x, y, x + sizeX, y + sizeY);
	}

    constexpr int width() const noexcept { return right - left; }
    constexpr int height() const noexcept { return down - top; }
	constexpr bool empty() const noexcept { return (right - left <= 0) || (down - top <= 0); }

	constexpr BoundingBox intersection(const BoundingBox& other) const noexcept {
		return {
			.left = std::max(left, other.left),
			.right = std::min(right, other.right),
			.top = std::max(top, other.top),
			.down = std::min(down, other.down),
		};
	}

    const BoundingBox unite_with(const BoundingBox& other) const noexcept {
        return {
            .left = std::min(left, other.left),
            .right = std::max(right, other.right),
            .top = std::min(top, other.top),
            .down = std::max(down, other.down),
        };
    }

    const BoundingBox extend(int x, int y) const noexcept {
	    return {
            .left = std::min(left, x),
            .right = std::max(right, x),
            .top = std::min(top, y),
            .down = std::max(down, y),
        };
	}

    constexpr BoundingBox getTranslated(int dx, int dy) const noexcept {
        return {
            .left = left + dx,
            .right = right + dx,
            .top = top + dy,
            .down = down + dy,
        };
    }

	constexpr bool isIntersects(const BoundingBox& other) const noexcept { return !intersection(other).empty(); }
	constexpr bool isPointInside(int x, int y) const noexcept { return (x >= left && x <= right && y >= top && y <= down); }
};
