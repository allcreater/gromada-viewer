module;
#include <flecs.h>
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
import engine.bounding_box;
import engine.level_renderer;
import engine.objects_view;

import Gromada.SoftwareRenderer;

constexpr ImVec2 to_imvec(const auto vec) { return ImVec2{static_cast<float>(vec.x), static_cast<float>(vec.y)}; }
constexpr glm::ivec2 from_imvec(const ImVec2 vec) { return glm::ivec2{static_cast<int>(vec.x), static_cast<int>(vec.y)}; }


export class MapViewModel {
public:
	explicit MapViewModel(Model& model)
		: m_model{model}, m_levelRenderer{model} {

	    m_selectionQuery = m_model.query_builder<const GameObject, const VidComponent>().with<Selected>().cached().build();
	}

	// actual range is from 1 to 8
	int magnificationFactor = 1;

	int animationFps = 10;

	glm::ivec2 screenToWorldPos(glm::ivec2 screenPos) const {
		return (m_camPos - m_viewportSize / 2) + (screenPos / magnificationFactor);
	}
	glm::ivec2 worldToScreenPos(glm::ivec2 worldPos) const {
		return  (worldPos - m_camPos + m_viewportSize / 2) * magnificationFactor;
	}

	void updateUI() {
		magnificationFactor = std::clamp(magnificationFactor, 1, 8);
		animationFps = std::clamp(animationFps, 1, 60);

		updateViewport();

		const auto mouseWorldPos = screenToWorldPos(from_imvec(ImGui::GetMousePos()));

		const auto* vp = ImGui::GetMainViewport();
		if (ImGui::BeginDragDropTargetCustom(ImRect{vp->WorkPos, vp->WorkSize}, vp->ID)) {
			ImGuiDragDropFlags target_flags = 0;
			target_flags |= ImGuiDragDropFlags_AcceptBeforeDelivery;
			//target_flags |= ImGuiDragDropFlags_AcceptNoDrawDefaultRect; // Don't display the yellow rectangle
			if (const auto payload = MyImUtils::AcceptDragDropPayload<ObjectToPlaceMessage>(target_flags)) {
				auto _ = m_model.entity()
			    .set<GameObject>({
					.nvid = payload->first.nvid,
					.x = mouseWorldPos.x,
					.y = mouseWorldPos.y,
					.z = 0,
					.direction = payload->first.direction,
				})
			    .add_if<DestroyAfterUpdate>(!(payload->second->IsDelivery()));

			}
			ImGui::EndDragDropTarget();
		}

		drawMap();

		if (!ImGui::IsDragDropActive()) {
			updateSelection(mouseWorldPos);
		}
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

	    // selection visualization
	    m_selectionQuery.each([&](const GameObject& obj, const VidComponent& vid) {
	        const glm::ivec2 halfSize {vid.vid().anotherWidth / 2, vid.vid().anotherHeight / 2};
            const glm::ivec2 pos {obj.x, obj.y};

            const auto color = objectSelectionColor(vid.vid().unitType);
            const float rounding = std::min(halfSize.x, halfSize.y) * 0.5f;
            draw_list->AddRectFilled(to_imvec(worldToScreenPos(pos - halfSize)), to_imvec(worldToScreenPos(pos + halfSize)), color, rounding);
	    });


		if (m_selectionFrame) {
			draw_list->AddRect(to_imvec(worldToScreenPos(m_selectionFrame->first)), to_imvec(worldToScreenPos(m_selectionFrame->second)), IM_COL32(0, 255, 0, 200));
		}

		std::array<char, 128> stringBuffer;
		draw_list->AddText({10, 30}, 0xFFFFFFFF, stringBuffer.data(), std::format_to(stringBuffer.data(), "map render time: {}", std::chrono::duration<float, std::milli>{renderDuration}));
	}

	void updateViewport() {
		m_viewportSize = from_imvec(ImGui::GetMainViewport()->Size) / magnificationFactor;

		if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
			m_camPos -= from_imvec(ImGui::GetIO().MouseDelta);
		}

	    const auto activeLevel = m_model.component<ActiveLevel>();
		if (const auto* mapHeader = activeLevel.get<MapHeaderRawData>()) {
			m_camPos = glm::clamp(m_camPos, glm::ivec2{0, 0}, glm::ivec2{mapHeader->width, mapHeader->height});
		}
	}

	void updateSelection(glm::ivec2 mouseWorldPos) {
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			if (!m_selectionFrame) {
				m_selectionFrame = std::pair{mouseWorldPos, mouseWorldPos};
			}
			else {
				m_selectionFrame->second = mouseWorldPos;
			    m_model.remove_all<Selected>();
                m_model.defer([&] {
                    m_model.get<ObjectsView>()->queryObjectsInRegion(ObjectsView::physicalBounds, BoundingBox::fromPositions(m_selectionFrame->first.x, m_selectionFrame->first.y, m_selectionFrame->second.x, m_selectionFrame->second.y), [](flecs::entity entity) {
                        entity.add<Selected>();
                    });
                });
			}
		}
		else if (m_selectionFrame) {
			m_selectionFrame.reset();
		}
	}

private:
	constexpr static ImU32 objectSelectionColor(UnitType unitType) {
		using enum UnitType;
		constexpr auto alpha = 70;
		switch (unitType) {
			case Terrain: return IM_COL32(50, 200, 50, alpha);
			case Object: return IM_COL32(200, 200, 200, alpha);
			case Monster: return IM_COL32(255, 100, 100, alpha);
			case Avia: return IM_COL32(50, 100, 200, alpha);
			case Cannon: return IM_COL32(128, 80, 50, alpha);
			case Sprite: return IM_COL32(100, 100, 100, alpha);
			case Item: return IM_COL32(200, 200, 50, alpha);
			default:
				return IM_COL32_BLACK;
		}
	}

private:
	Model& m_model;

	glm::ivec2 m_camPos;
	glm::ivec2 m_viewportSize;

	std::optional<std::pair<glm::ivec2, glm::ivec2>> m_selectionFrame;
    flecs::query<const GameObject, const VidComponent> m_selectionQuery;
	LevelRenderer m_levelRenderer;
	Framebuffer m_levelFramebuffer;
};