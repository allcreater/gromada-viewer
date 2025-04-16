module;
#include <glm/glm.hpp>
#include <imgui.h>
#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_glue.h>
#include <util/sokol_imgui.h>

export module application.view_model : map;

import std;
import sokol.helpers;
import imgui_utils;

import application.model;

import Gromada.SoftwareRenderer;
import Gromada.VisualLogic;

constexpr ImVec2 to_imvec(const auto vec) { return ImVec2{static_cast<float>(vec.x), static_cast<float>(vec.y)}; }
constexpr glm::ivec2 from_imvec(const ImVec2 vec) { return glm::ivec2{static_cast<int>(vec.x), static_cast<int>(vec.y)}; }

export class MapViewModel {
public:
	explicit MapViewModel(Model& model)
		: m_model{model}, m_camPos{model.map().header.observerX, model.map().header.observerY} {}

	// actual range is from 1 to 8
	int magnificationFactor = 1;

	int animationFps = 16;

	void drawMap() {
		magnificationFactor = std::clamp(magnificationFactor, 1, 8);
		animationFps = std::clamp(animationFps, 1, 60);

		const auto screenSize = from_imvec(ImGui::GetMainViewport()->Size) / magnificationFactor;
		const auto camOffset = m_camPos - screenSize / 2;
		
		prepareFramebuffer(screenSize);

		const auto frameCounter = std::chrono::steady_clock::now().time_since_epoch() / std::chrono::milliseconds(1000 / animationFps);

		updateObjectsView(screenSize);
		for (const auto& [vid, spritesPack, obj, pos] : m_visibleObjects) {
			auto [minIndex, maxIndex] = getAnimationFrameRange(*vid, Action::Stand, obj->direction);
			assert(maxIndex < spritesPack->numOfFrames);

			const auto animationOffset = frameCounter + static_cast<std::uint32_t>(reinterpret_cast<const std::uintptr_t>(obj));
			const auto frameNumber = animationOffset % (maxIndex - minIndex + 1) + minIndex;
			DrawSprite(*spritesPack, frameNumber, pos.x, pos.y, m_framebuffer->dataDesc);
		}

		m_framebuffer->commitToGpu();

		ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
		draw_list->AddImage(simgui_imtextureid(m_framebuffer->image), ImVec2{0, 0}, 
			ImGui::GetMainViewport()->Size,
			ImVec2{0, 0},
			ImVec2{1, 1}, IM_COL32(255, 255, 255, 255));

		updateCameraPos();
	}

	void prepareFramebuffer(glm::ivec2 screenSize) {
		if (!m_framebuffer || m_framebuffer->dataDesc.extent(0) != screenSize.y || m_framebuffer->dataDesc.extent(1) != screenSize.x) {
			m_framebuffer = Framebuffer{screenSize.x, screenSize.y};
		}

		std::ranges::fill(m_framebuffer->data, RGBA8{0, 0, 0, 0});
	}

	void updateObjectsView(glm::ivec2 screenSize) {
		const auto makeObjectView = [this, camOffset = m_camPos - screenSize / 2](const GameObject& obj) -> ObjectView {
			const auto& spritesPack = m_model.getVidGraphics(obj.nvid);
			const glm::ivec2 pos = glm::ivec2{obj.x - spritesPack.imgWidth / 2, obj.y - spritesPack.imgHeight / 2} - camOffset;
			return {
				.pVid = &m_model.vids()[obj.nvid],
				.pSpritesPack = &spritesPack,
				.pObj = &obj,
				.screenPos = pos,
			};
		};

		const auto isVisible = [screenSize](const ObjectView& obj) {
			const auto size = glm::ivec2{obj.pSpritesPack->imgWidth, obj.pSpritesPack->imgHeight};
			return obj.screenPos.x + size.x >= 0 && obj.screenPos.x < screenSize.x && obj.screenPos.y + size.y >= 0 &&
				   obj.screenPos.y < screenSize.y;
		};

		auto objects = m_model.map().objects
			| std::views::transform(makeObjectView)
			| std::views::filter(isVisible)
			| std::views::common;
		m_visibleObjects.assign(objects.begin(), objects.end());
		std::ranges::sort(m_visibleObjects, {},
			[](const ObjectView& obj) { return std::tuple{obj.pSpritesPack->visualBehavior, obj.screenPos.y + obj.pSpritesPack->imgHeight / 2}; });
	}

	void updateCameraPos() {
		if (ImGui::IsMouseDragging(1)) {
			m_camPos -= from_imvec(ImGui::GetIO().MouseDelta);
		}

		const auto& map = m_model.map();
		m_camPos = glm::clamp(m_camPos, glm::ivec2{0, 0}, glm::ivec2{map.header.width, map.header.height});
	}

private:
	Model& m_model;

	glm::ivec2 m_camPos;

	struct ObjectView {
		const Vid* pVid;
		const VidGraphics* pSpritesPack;
		const GameObject* pObj;
		glm::ivec2 screenPos;
	};
	std::vector<ObjectView> m_visibleObjects;

	struct Framebuffer {
		Framebuffer(int width, int height)
			: image{sg_image_desc{
				  .type = SG_IMAGETYPE_2D,
				  .width = width,
				  .height = height,
				  .usage = SG_USAGE_STREAM,
				  .pixel_format = SG_PIXELFORMAT_RGBA8,
			  }},
			  data{static_cast<size_t>(width * height), RGBA8{0, 0, 0, 0}},
			  dataDesc{data.data(), std::dextents<int, 2>{height, width}} {}

		void commitToGpu() { sg_update_image(image, sg_image_data{{{{.ptr = data.data(), .size = data.size() * sizeof(RGBA8)}}}}); }

		SgUniqueImage image;
		std::vector<RGBA8> data;
		FramebufferRef dataDesc;
	};
	std::optional<Framebuffer> m_framebuffer;
};