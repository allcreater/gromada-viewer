module;
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_glue.h>
#include <util/sokol_imgui.h>

export module application.view_model : map;

import std;
import imgui_utils;

import application.model;
import level_renderer;

import Gromada.SoftwareRenderer;
import sokol.helpers;

constexpr ImVec2 to_imvec(const auto vec) { return ImVec2{static_cast<float>(vec.x), static_cast<float>(vec.y)}; }
constexpr glm::ivec2 from_imvec(const ImVec2 vec) { return glm::ivec2{static_cast<int>(vec.x), static_cast<int>(vec.y)}; }

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
		  dataDesc{data.data(), std::dextents<int, 2>{height, width}}
	{}

	void commitToGpu() { sg_update_image(image, sg_image_data{{{{.ptr = data.data(), .size = data.size() * sizeof(RGBA8)}}}}); }

	operator FramebufferRef(){
		return dataDesc;
	}

	SgUniqueImage image;
	std::vector<RGBA8> data;
	FramebufferRef dataDesc;
};


export class MapViewModel {
public:
	explicit MapViewModel(Model& model)
		: m_model{model}, m_camPos{model.map().header.observerX, model.map().header.observerY}, m_levelRenderer{model} {}

	// actual range is from 1 to 8
	int magnificationFactor = 1;

	int animationFps = 16;

	glm::ivec2 screenToWorldPos(glm::ivec2 screenPos) const {
		const auto* vp = ImGui::GetMainViewport();
		return (m_camPos - m_viewportSize / 2) + (screenPos / magnificationFactor);
	}

	void updateUI() {
		magnificationFactor = std::clamp(magnificationFactor, 1, 8);
		animationFps = std::clamp(animationFps, 1, 60);

		m_viewportSize = from_imvec(ImGui::GetMainViewport()->Size) / magnificationFactor;
		drawMap();

		//if (ImGui::BeginDragDropTarget()) {

		const auto* vp = ImGui::GetMainViewport();
		if (ImGui::BeginDragDropTargetCustom(ImRect{vp->WorkPos, vp->WorkSize}, vp->ID)) {
			ImGuiDragDropFlags target_flags = 0;
			//target_flags |= ImGuiDragDropFlags_AcceptBeforeDelivery;	// Don't wait until the delivery (release mouse button on a target) to do something
			//target_flags |= ImGuiDragDropFlags_AcceptNoDrawDefaultRect; // Don't display the yellow rectangle
			if (const auto payload = MyImUtils::AcceptDragDropPayload<ObjectToPlaceMessage>(target_flags)) {
				const auto pos = screenToWorldPos(from_imvec(ImGui::GetMousePos()));
				m_model.map().objects.push_back({
					.nvid = payload->first.nvid,
					.x = pos.x,
					.y = pos.y,
					.z = 0,
					.direction = 0,
				});
			}
			ImGui::EndDragDropTarget();
		}
		updateCameraPos();
	}

private:
	void drawMap() {
		prepareFramebuffer(m_viewportSize);
		assert(m_levelFramebuffer);

		const auto frameCounter = std::chrono::steady_clock::now().time_since_epoch() / std::chrono::milliseconds(1000 / animationFps);
		m_levelRenderer.drawMap(*m_levelFramebuffer, (m_camPos - m_viewportSize / 2), m_viewportSize, frameCounter);
		
		m_levelFramebuffer->commitToGpu();
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		draw_list->AddImage(
			simgui_imtextureid(m_levelFramebuffer->image), ImVec2{0, 0}, 
			ImGui::GetMainViewport()->Size,
			ImVec2{0, 0},
			ImVec2{1, 1}, IM_COL32(255, 255, 255, 255));

		//ImGui::Image(simgui_imtextureid(m_levelFramebuffer->image), ImGui::GetMainViewport()->Size);
	}

	void prepareFramebuffer(glm::ivec2 viewportSize) {
		if (!m_levelFramebuffer || m_levelFramebuffer->dataDesc.extent(0) != viewportSize.y || m_levelFramebuffer->dataDesc.extent(1) != viewportSize.x) {
			m_levelFramebuffer = Framebuffer{viewportSize.x, viewportSize.y};
		}

		std::ranges::fill(m_levelFramebuffer->data, RGBA8{0, 0, 0, 0});
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
	glm::ivec2 m_viewportSize;

	LevelRenderer m_levelRenderer;
	std::optional<Framebuffer> m_levelFramebuffer;
};