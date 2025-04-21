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
import framebuffer;
import imgui_utils;

import application.model;
import engine.level_renderer;

import Gromada.SoftwareRenderer;

constexpr ImVec2 to_imvec(const auto vec) { return ImVec2{static_cast<float>(vec.x), static_cast<float>(vec.y)}; }
constexpr glm::ivec2 from_imvec(const ImVec2 vec) { return glm::ivec2{static_cast<int>(vec.x), static_cast<int>(vec.y)}; }


export class MapViewModel {
public:
	explicit MapViewModel(Model& model)
		: m_model{model}, m_camPos{model.map().header.observerX, model.map().header.observerY}, m_levelRenderer{model.objectsView()} {}

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

		const size_t numberOfObjectsBeforeModification = m_model.map().objects.size();

		const auto* vp = ImGui::GetMainViewport();
		if (ImGui::BeginDragDropTargetCustom(ImRect{vp->WorkPos, vp->WorkSize}, vp->ID)) {
			ImGuiDragDropFlags target_flags = 0;
			target_flags |= ImGuiDragDropFlags_AcceptBeforeDelivery;
			//target_flags |= ImGuiDragDropFlags_AcceptNoDrawDefaultRect; // Don't display the yellow rectangle
			if (const auto payload = MyImUtils::AcceptDragDropPayload<ObjectToPlaceMessage>(target_flags)) {
				const auto pos = screenToWorldPos(from_imvec(ImGui::GetMousePos()));
				m_model.map().objects.push_back({
					.nvid = payload->first.nvid,
					.x = pos.x,
					.y = pos.y,
					.z = 0,
					.direction = payload->first.direction,
				});
			}
			ImGui::EndDragDropTarget();
		}

		drawMap();

		// Roll back objects on map if object is not dropped yet
		if (ImGui::IsDragDropActive() && (m_model.map().objects.size() > numberOfObjectsBeforeModification)) {
			m_model.map().objects.resize(numberOfObjectsBeforeModification);
		}

		updateCameraPos();
	}

private:
	void drawMap() {
		m_levelFramebuffer.resize(m_viewportSize);
		m_levelFramebuffer.clear({0, 0, 0, 0});

		const auto frameCounter = std::chrono::steady_clock::now().time_since_epoch() / std::chrono::milliseconds(1000 / animationFps);

		const auto time = std::chrono::high_resolution_clock::now();
		m_levelRenderer.drawMap(m_levelFramebuffer, (m_camPos - m_viewportSize / 2), m_viewportSize, frameCounter);
		const auto renderDuration = std::chrono::high_resolution_clock::now() - time;
		
		m_levelFramebuffer.commitToGpu();
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		draw_list->AddImage(
			simgui_imtextureid(m_levelFramebuffer.getImage()), ImVec2{0, 0}, 
			ImGui::GetMainViewport()->Size,
			ImVec2{0, 0},
			ImVec2{1, 1}, IM_COL32(255, 255, 255, 255));

		std::array<char, 128> stringBuffer;
		draw_list->AddText({10, 30}, 0xFFFFFFFF, stringBuffer.data(), std::format_to(stringBuffer.data(), "map render time: {}", duration_cast<std::chrono::milliseconds>(renderDuration)));
		//ImGui::Image(simgui_imtextureid(m_levelFramebuffer.getImage()), ImGui::GetMainViewport()->Size);
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
	Framebuffer m_levelFramebuffer;
};