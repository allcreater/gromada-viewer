module;
#include <flecs.h>
#include <glm/glm.hpp>
#include <imgui.h>

export module application.view_model;

import std;
import imgui_utils;
import Gromada.DataExporters;

import engine.level_renderer; // For Viewport. Better to split

import application.model;
import :map;
import :map_selector;
import :vids_window;
import :map_properties;
import :sounds_window;
import :dialogs;

export class ViewModel {
public:
	explicit ViewModel(Model& model)
		: m_model{model} {

	    m_model.newMap({}, 10, 10);
	}

	void updateUI() {
		drawMenu();
		const auto* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		// ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::Begin("Root window", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);

		m_mapViewModel.updateUI();

		ImGui::SetNextWindowPos({10, 20}, ImGuiCond_Appearing);
		ImGui::SetNextWindowSize(ImVec2{300, 500}, ImGuiCond_Appearing);
		ImGui::Begin("Panel");
		if (ImGui::BeginTabBar("Tabs")) {
			if (ImGui::BeginTabItem("Vids")) {
				m_vidsViewModel.updateUI();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Maps")) {
				m_mapsSelectorViewModel.updateUI();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Map properties")) {
				m_mapPropertiesViewModel.updateUI();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Sounds")) {
				m_soundsViewModel.updateUI();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Help")) {
				ImGui::TextUnformatted(
					R"(
Controls:
- Ctrl+Wheel - zoom in/out
- Right mouse button / Ctrl + mouse - move camera
- Left mouse button - select object
- Del - delete selected objects
)");

				ImGui::Separator();

				ImGui::Text("Version: %s", BUILD_INFO_PROJECT_VERSION);
				ImGui::Text("Build time: %s", BUILD_INFO_TIMESTAMP);
				ImGui::Text("Commit: %s", BUILD_INFO_COMMIT_HASH);

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
		ImGui::End();

		ImGui::End();

		// ImGui::ShowDemoWindow();
	}

	void drawMenu() {
	    const auto vids = m_model.get<const GameResources>().vids();

		ImGui::BeginMainMenuBar();
		if (ImGui::BeginMenu("File")) {
		    if (ImGui::MenuItem("New map")) {
		        m_newMapDialog.open();
            }

			if (ImGui::MenuItem("Export map JSON")) {
				m_exportMapDialog.open(m_model);
			}

		    if (ImGui::MenuItem("Save map")) {
		        std::ofstream file {"maps/EXPERIMENTAL_SAVE.map", std::ios_base::out | std::ios_base::binary};
		        saveMap(vids, m_model.saveMap(), file);
		    }

			// TODO: reuse popup from previous item
			if (ImGui::MenuItem("Export vids to CSV")) {
				std::ofstream stream{"vids.csv", std::ios_base::out};
				stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

				ExportVidsToCsv(vids, stream);
			}

			if (ImGui::MenuItem("Exit")) {
				throw std::runtime_error("Exit requested");
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Map")) {
		    m_mapViewModel.onMenu();
		    ImGui::EndMenu();
		}

		m_exportMapDialog.updateUI(vids, m_model);
		m_newMapDialog.updateUI(m_model);

		{
			ImGui::SameLine( 0, 50 );
			ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2(2, 2) );

			auto& state = m_model.get_mut<GlobalEditorState>().state;
			MyImUtils::ToggleButton("Select", std::holds_alternative<SelectionState>(state), [&](){
				state = SelectionState{};
				m_model.modified<GlobalEditorState>();
			});

			ImGui::SameLine();

			MyImUtils::ToggleButton("Place", std::holds_alternative<PlacementState>(state), [&](){
				state = PlacementState{};
				m_model.modified<GlobalEditorState>();
			});

			ImGui::PopStyleVar();
		}

		{
		    ImGui::SameLine(ImGui::GetWindowWidth() - 150);
	        const auto& vp = m_model.get<const Viewport>();
		    const auto pos = vp.screenToWorldPos(from_imvec(ImGui::GetMousePos()));
		    ImGui::Text("x: %i, y: %i, zoom: %i", static_cast<int>(pos.x), static_cast<int>(pos.y), m_model.get<const Camera>().magnificationFactor);
	    }

		ImGui::EndMainMenuBar();
	}

private:
	Model& m_model;

	VidsWindowViewModel m_vidsViewModel{m_model};
	MapViewModel m_mapViewModel{m_model};
	MapsSelectorViewModel m_mapsSelectorViewModel{m_model};
    MapPropertiesViewModel m_mapPropertiesViewModel{m_model};
    SoundsWindowViewModel m_soundsViewModel{m_model};

    NewMapDialog m_newMapDialog;
    ExportMapDialog m_exportMapDialog;
};