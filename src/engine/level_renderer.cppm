module;
#include <glm/glm.hpp>

export module engine.level_renderer;

import std;

import engine.bounding_box;
import engine.objects_view;

import Gromada.SoftwareRenderer;
import Gromada.VisualLogic;

// TODO: struct Viewport?

export class LevelRenderer {
public:
	LevelRenderer(const ObjectsView& objectsView)
		: m_objectsView{objectsView} {}

	void drawMap(FramebufferRef framebuffer, glm::ivec2 viewportOffset, glm::ivec2 viewportSize, std::uint32_t frameCounter) {
		updateObjectsView(viewportOffset, viewportOffset + viewportSize);
		for (const auto& [vid, spritesPack, obj, _] : m_visibleObjects) {
			const glm::ivec2 pos = glm::ivec2{obj.get().x - spritesPack.get().imgWidth / 2, obj.get().y - spritesPack.get().imgHeight / 2} - viewportOffset;
			auto [minIndex, maxIndex] = getAnimationFrameRange(vid, Action::Stand, obj.get().direction);
			assert(maxIndex < spritesPack.get().numOfFrames);

			const auto animationOffset = frameCounter + static_cast<std::uint32_t>(reinterpret_cast<const std::uintptr_t>(&(obj.get())) / 57);
			const auto frameNumber = animationOffset % (maxIndex - minIndex + 1) + minIndex;
			DrawSprite(spritesPack, frameNumber, pos.x, pos.y, framebuffer);
		}
	}

private:
	void updateObjectsView(glm::ivec2 min, glm::ivec2 max) {
		m_visibleObjects.assign_range(m_objectsView.queryObjectsInRegion(BoundingBox{min.x, max.x, min.y, max.y}));
		std::ranges::sort(m_visibleObjects, {},
			[](const ObjectView& obj) { return std::tuple{obj.graphics.get().visualBehavior, obj.obj.get().y + obj.graphics.get().imgHeight}; });
	}

private:
	const ObjectsView& m_objectsView;
	std::vector<ObjectView> m_visibleObjects;
};