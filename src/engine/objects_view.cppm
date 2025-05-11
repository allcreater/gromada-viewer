module;
#include <glm/glm.hpp>

export module engine.objects_view;

import std;
import utils;

import engine.bounding_box;

import Gromada.GameResources;
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

struct VisualBoundsFn {
	BoundingBox operator()(const ObjectView& view) const {
		return getCenteredBB({view.obj.get().x, view.obj.get().y}, {view.graphics.get().imgWidth, view.graphics.get().imgHeight});
	}
};
struct PhysicalBoundsFn {
	BoundingBox operator()(const ObjectView& view) const {
		return getCenteredBB({view.obj.get().x, view.obj.get().y}, {view.vid.get().anotherWidth, view.vid.get().anotherHeight});
	}
};

// "A poor's man World" or something like that :) Will be supplemented as needed
class ObjectsView {
public:
	struct VisualBounds {};
	struct PhysicalBounds {};
	constexpr static inline VisualBounds visualBounds {}; 
	constexpr static inline PhysicalBounds physicalBounds{}; 

	ObjectsView(const GameResources& resources, const Map& map)
		: m_resources{resources}, m_map{map} {}

	void update() {
		const auto makeObjectView = [this](const GameObject& obj) -> ObjectView {
		    const auto& [vid, graphics] = m_resources.getVid(obj.nvid);
			return {
				.vid = vid,
				.graphics = graphics,
				.obj = obj,
				.objectIndex = static_cast<size_t>(std::distance(m_map.objects.data(), &obj)),
			};
		};

		m_objects.assign_range(m_map.objects | std::views::transform(makeObjectView));
	}

	inline std::ranges::bidirectional_range auto queryObjectsInRegion([[maybe_unused]] VisualBounds, BoundingBox region) const {
		return m_objects | std::views::filter([region, boundsFn = VisualBoundsFn{}](
												  const ObjectView& object) { return boundsFn(object).isIntersects(region); });
	}

	inline std::ranges::bidirectional_range auto queryObjectsInRegion([[maybe_unused]] PhysicalBounds, BoundingBox region) const {
		return m_objects | std::views::filter([region, boundsFn = PhysicalBoundsFn{}](
												  const ObjectView& object) { return boundsFn(object).isIntersects(region); });
	}

	std::span<const ObjectView> objects() const noexcept { return m_objects; }

private:
    const GameResources& m_resources;
	const Map& m_map;

	std::vector<ObjectView> m_objects;
};

}
