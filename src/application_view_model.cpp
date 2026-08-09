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
		: m_model{model}
		, m_exportMapDialog{std::bind_front(&ViewModel::exportMapAsJson, this), "Export Map"}
		, m_saveMapDialog{std::bind_front(&ViewModel::saveMapToFile, this), "Save Map"}
		, m_exportVidsDialog{std::bind_front(&ViewModel::exportVidsToCsv, this), "Export Vids CSV"}
	{
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
		ImGui::BeginMainMenuBar();
		if (ImGui::BeginMenu("File")) {
		    if (ImGui::MenuItem("New map")) {
		        m_newMapDialog.open();
            }

			if (ImGui::MenuItem("Export map JSON")) {
				const auto* activeMapPath = m_model.component<ActiveLevel>().try_get<Path>();
				m_exportMapDialog.open(activeMapPath ? std::format("{}.json", activeMapPath->stem().generic_string()) : std::string{"map.json"});
			}

		    if (ImGui::MenuItem("Save map")) {
		    	const auto& mapsPath = m_model.get<const GameResources>().mapsPath();

		    	const auto* activeMapPath = m_model.component<ActiveLevel>().try_get<Path>();
		        m_saveMapDialog.open(activeMapPath && !activeMapPath->empty() ? activeMapPath->generic_string() : (mapsPath / "NEW_MAP.map").generic_string());
		    }

			// TODO: reuse popup from previous item
			if (ImGui::MenuItem("Export vids to CSV")) {
				m_exportVidsDialog.open("vids.csv");
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

		m_newMapDialog.updateUI(m_model);
		m_exportMapDialog.updateUI();
		m_saveMapDialog.updateUI();
		m_exportVidsDialog.updateUI();

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
	void exportMapAsJson(std::ostream&& stream) const {
		ExportMapToJson(m_model.get<const GameResources>().vids(), m_model.saveMap(), stream);
	}

	void saveMapToFile(std::ostream&& stream) const {
		saveMap(m_model.get<const GameResources>().vids(), m_model.saveMap(), stream);
	}

	void exportVidsToCsv(std::ostream&& stream) const {
		ExportVidsToCsv(m_model.get<const GameResources>().vids(), stream);
	}

private:
	Model& m_model;

	VidsWindowViewModel m_vidsViewModel{m_model};
	MapViewModel m_mapViewModel{m_model};
	MapsSelectorViewModel m_mapsSelectorViewModel{m_model};
    MapPropertiesViewModel m_mapPropertiesViewModel{m_model};
    SoundsWindowViewModel m_soundsViewModel{m_model};

    NewMapDialog m_newMapDialog;
    SaveDialog m_exportMapDialog;
    SaveDialog m_saveMapDialog;
    SaveDialog m_exportVidsDialog;
};