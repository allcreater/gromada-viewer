module;
#include <glm/glm.hpp>
export module level_renderer;

import std;

import application.model;
import Gromada.SoftwareRenderer;
import Gromada.VisualLogic;

// TODO: struct Viewport?

export class LevelRenderer {
public:
	LevelRenderer(const Model& model)
		: m_model{model} {}

	void drawMap(FramebufferRef framebuffer, glm::ivec2 viewportOffset, glm::ivec2 viewportSize, std::uint32_t frameCounter) {
		updateObjectsView(viewportOffset, viewportSize);
		for (const auto& [vid, spritesPack, obj, pos] : m_visibleObjects) {
			auto [minIndex, maxIndex] = getAnimationFrameRange(*vid, Action::Stand, obj->direction);
			assert(maxIndex < spritesPack->numOfFrames);

			const auto animationOffset = frameCounter + static_cast<std::uint32_t>(reinterpret_cast<const std::uintptr_t>(obj) / alignof(std::uintptr_t));
			const auto frameNumber = animationOffset % (maxIndex - minIndex + 1) + minIndex;
			DrawSprite(*spritesPack, frameNumber, pos.x, pos.y, framebuffer);
		}
	}

private:
	void updateObjectsView(glm::ivec2 viewportOffset, glm::ivec2 viewportSize) {
		const auto makeObjectView = [this, viewportOffset](const GameObject& obj) -> ObjectView {
			const auto& spritesPack = m_model.getVidGraphics(obj.nvid);
			const glm::ivec2 pos = glm::ivec2{obj.x - spritesPack.imgWidth / 2, obj.y - spritesPack.imgHeight / 2} - viewportOffset;
			return {
				.pVid = &m_model.vids()[obj.nvid],
				.pSpritesPack = &spritesPack,
				.pObj = &obj,
				.screenPos = pos,
			};
		};

		const auto isVisible = [viewportSize](const ObjectView& obj) {
			const auto size = glm::ivec2{obj.pSpritesPack->imgWidth, obj.pSpritesPack->imgHeight};
			return obj.screenPos.x + size.x >= 0 && obj.screenPos.x < viewportSize.x && obj.screenPos.y + size.y >= 0 && obj.screenPos.y < viewportSize.y;
		};

		auto objects = m_model.map().objects | std::views::transform(makeObjectView) | std::views::filter(isVisible) | std::views::common;
		m_visibleObjects.assign(objects.begin(), objects.end());
		std::ranges::sort(m_visibleObjects, {},
			[](const ObjectView& obj) { return std::tuple{obj.pSpritesPack->visualBehavior, obj.screenPos.y + obj.pSpritesPack->imgHeight / 2}; });
	}

private:
	const Model& m_model;

	struct ObjectView {
		const Vid* pVid;
		const VidGraphics* pSpritesPack;
		const GameObject* pObj;
		glm::ivec2 screenPos;
	};
	std::vector<ObjectView> m_visibleObjects;
};