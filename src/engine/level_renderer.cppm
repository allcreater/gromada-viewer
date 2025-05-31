module;
#include <flecs.h>
#include <glm/glm.hpp>

export module engine.level_renderer;

import std;

import engine.bounding_box;
import engine.objects_view;
import engine.world_components;

import Gromada.Resources;
import Gromada.SoftwareRenderer;
import Gromada.VisualLogic;

// TODO: struct Viewport?

export class LevelRenderer {
public:
	LevelRenderer(const flecs::world& world)
		: m_world{world} {}

	void drawMap(FramebufferRef framebuffer, glm::ivec2 viewportOffset, glm::ivec2 viewportSize) {
		updateObjectsView(viewportOffset, viewportOffset + viewportSize);
		for (flecs::entity entity : m_visibleObjects) {
		    auto obj = entity.get_ref<const GameObject>();
		    auto vid = entity.get_ref<const Vid>();
		    auto animation = entity.get_ref<const AnimationComponent>();

			const glm::ivec2 pos = glm::ivec2{obj->x - vid->graphics().width / 2, obj->y - vid->graphics().height / 2} - viewportOffset;
			DrawSprite(vid->graphics(), animation->current_frame, pos.x, pos.y, framebuffer);
		}
	}

private:
	void updateObjectsView(glm::ivec2 min, glm::ivec2 max) {
	    m_visibleObjects.clear();
	    m_world.get<ObjectsView>()->queryObjectsInRegion(ObjectsView::visualBounds, BoundingBox{min.x, max.x, min.y, max.y}, [&](flecs::entity entity){ m_visibleObjects.push_back(entity); });

		std::ranges::sort(m_visibleObjects, {},
			[](flecs::entity entity) {
			    auto obj = entity.get_ref<const GameObject>();
			    auto vid = entity.get_ref<const Vid>();
			    return std::tuple{vid->z_layer, obj->y + vid->graphics().height / 2};
			});
	}

private:
	const flecs::world& m_world;
	std::vector<flecs::entity> m_visibleObjects;
};