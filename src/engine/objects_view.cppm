module;
#include <glm/glm.hpp>

export module engine.objects_view;

import std;
import utils;

import engine.bounding_box;

import Gromada.Resources;
import Gromada.VisualLogic;

BoundingBox getCenteredBB(glm::ivec2 pos, glm::ivec2 size) {
	const auto min = pos - size/2, max = pos + size/2;
	return BoundingBox{
		.left = min.x,
		.right = max.x,
		.top = min.y,
		.down = max.y,
	};
}

export {

struct ObjectView {
	std::reference_wrapper<const Vid> vid;
	std::reference_wrapper<const VidGraphics> graphics;
	std::reference_wrapper<const GameObject> obj;
	size_t objectIndex;
};

struct VisualBounds {
	BoundingBox operator()(const ObjectView& view) const {
		return getCenteredBB({view.obj.get().x, view.obj.get().y}, {view.graphics.get().imgWidth, view.graphics.get().imgHeight});
	}
};
constexpr inline VisualBounds visualBounds{};

struct PhysicalBounds {
	BoundingBox operator()(const ObjectView& view) const {
		return getCenteredBB({view.obj.get().x, view.obj.get().y}, {view.vid.get().anotherWidth, view.vid.get().anotherHeight});
	}
};
constexpr inline PhysicalBounds physicalBounds{};

// "A poor's man World" or something like that :) Will be supplemented as needed
class ObjectsView {


public:
	ObjectsView(std::span<const Vid> vids, const Map& map)
		: m_vids{vids}, m_map{map} {}

	void update() {
		const auto makeObjectView = [this](const GameObject& obj) -> ObjectView {
			return {
				.vid = m_vids[obj.nvid],
				.graphics = getVidGraphics(m_vids, obj.nvid),
				.obj = obj,
				.objectIndex = static_cast<size_t>(std::distance(m_map.objects.data(), &obj)),
			};
		};

		m_objects.assign_range(m_map.objects | std::views::transform(makeObjectView));
	}

	template <typename BoundsFn = decltype(visualBounds)>
	inline std::ranges::bidirectional_range auto queryObjectsInRegion(BoundingBox region, BoundsFn&& boundsFn = BoundsFn{}) const {
		return m_objects | std::views::filter([region, boundsFn = std::forward<BoundsFn>(boundsFn)](
												  const ObjectView& object) { return boundsFn(object).isIntersects(region); });
	}

	std::span<const ObjectView> objects() const { return m_objects; }

private:
	std::span<const Vid> m_vids;
	const Map& m_map;

	std::vector<ObjectView> m_objects;
};

}
