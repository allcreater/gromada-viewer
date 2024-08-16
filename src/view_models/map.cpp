module;
#include <imgui.h>
#include <glm/glm.hpp>

export module application.view_model : map;

import std;
import sokol;
import sokol.imgui;
import imgui_utils;

import application.model;

constexpr ImVec2 to_imvec(const auto vec) { return ImVec2{static_cast<float>(vec.x), static_cast<float>(vec.y)}; }
constexpr glm::ivec2 from_imvec(const ImVec2 vec) { return glm::ivec2{static_cast<int>(vec.x), static_cast<int>(vec.y)}; }

export class MapViewModel {
private:
	struct VidView {
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

		const auto& map = m_model.map();
		for (const auto& obj : map.objects()) {
			const auto vid = m_vids[obj.nvid];

			const glm::ivec2 pos = glm::ivec2{obj.x, obj.y} - camOffset;
			const glm::ivec2 size{vid.pVid->anotherWidth, vid.pVid->anotherHeight};
			draw_list->AddRectFilled(to_imvec(pos), to_imvec(pos + size), IM_COL32(255, 0, 0, 50));
		}
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
};