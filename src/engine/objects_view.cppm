module;
#include <glm/glm.hpp>
#include <flecs.h>

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

struct VisualBoundsFn {
	BoundingBox operator()(const Vid& vid, const GameObject& obj) const {
		return getCenteredBB({obj.x, obj.y}, {vid.graphics().imgWidth, vid.graphics().imgHeight});
	}
};
struct PhysicalBoundsFn {
	BoundingBox operator()(const Vid& vid, const GameObject& obj) const {
		return getCenteredBB({obj.x, obj.y}, {vid.anotherWidth, vid.anotherHeight});
	}
};

class ObjectsView {
public:
	struct VisualBounds {};
	struct PhysicalBounds {};
	constexpr static inline VisualBounds visualBounds{};
	constexpr static inline PhysicalBounds physicalBounds{};

	ObjectsView(flecs::world& world) { m_query = world.query_builder<const Vid, const GameObject>().cached().build(); }

    inline void queryObjectsInRegion([[maybe_unused]] VisualBounds, BoundingBox region, const auto& callback) const {
	    queryObjectsInRegionImpl(VisualBoundsFn{}, region, callback);
	}

	 inline void queryObjectsInRegion([[maybe_unused]] PhysicalBounds, BoundingBox region, const auto& callback) const {
	    queryObjectsInRegionImpl(PhysicalBoundsFn{}, region, callback);
	 }
private:
    void queryObjectsInRegionImpl(auto&& boundsFn, BoundingBox region, const auto& callback) const {
        m_query.each([region, boundsFn, &callback](flecs::entity entity, const Vid& vid, const GameObject& obj) {
            if (boundsFn(vid, obj).isIntersects(region)) {
                std::invoke(callback,entity/*, vid, obj*/);
            }
        });

    }

private:
	flecs::query<const Vid, const GameObject> m_query;
};

}
