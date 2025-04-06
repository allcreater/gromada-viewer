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

constexpr ImVec2 to_imvec(const auto vec) { return ImVec2{static_cast<float>(vec.x), static_cast<float>(vec.y)}; }
constexpr glm::ivec2 from_imvec(const ImVec2 vec) { return glm::ivec2{static_cast<int>(vec.x), static_cast<int>(vec.y)}; }

export class MapViewModel {
private:
	struct VidView { // TODO: unused
		const VidRawData* pVid;
	};

public:
	explicit MapViewModel(Model& model)
		: m_model{model}, m_vids{getVids(model)}, m_camPos{model.map().header().observerX, model.map().header().observerY} {}

	static std::vector<VidView> getVids(const Model& model) {
		return model.vids() | std::views::transform([](const VidRawData& vid) { return VidView{&vid}; }) | std::ranges::to<std::vector>();
	}

	void drawMap() {
		ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

		const auto screenSize = from_imvec(ImGui::GetMainViewport()->Size);
		const auto camOffset = m_camPos - screenSize / 2;

		if (!m_framebuffer || m_framebuffer->dataDesc.extent(0) != screenSize.y || m_framebuffer->dataDesc.extent(1) != screenSize.x) {
			m_framebuffer = Framebuffer{screenSize.x, screenSize.y};
		}

		std::ranges::fill(m_framebuffer->data, RGBA8{0, 0, 0, 0});

		const auto& map = m_model.map();
		for (const auto& obj : map.objects()) {
			const auto& spritesPack = m_model.getVidGraphics(obj.nvid);

			const glm::ivec2 pos = glm::ivec2{obj.x, obj.y} - camOffset;
			DrawSprite(spritesPack, 0, pos.x - spritesPack.imgWidth / 2, pos.y - spritesPack.imgHeight / 2, m_framebuffer->dataDesc);
		}

		m_framebuffer->commitToGpu();
		draw_list->AddImage(simgui_imtextureid(m_framebuffer->image), ImVec2{0, 0}, ImVec2{static_cast<float>(screenSize.x), static_cast<float>(screenSize.y)},
			ImVec2{0, 0},
			ImVec2{1, 1}, IM_COL32(255, 255, 255, 255));

		updateCameraPos();
	}

	void updateCameraPos() {
		if (ImGui::IsMouseDragging(1)) {
			m_camPos -= from_imvec(ImGui::GetIO().MouseDelta);
		}

		const auto& map = m_model.map();
		m_camPos = glm::clamp(m_camPos, glm::ivec2{0, 0}, glm::ivec2{map.header().width, map.header().height});
	}

private:
	Model& m_model;

	std::vector<VidView> m_vids;
	glm::ivec2 m_camPos;

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