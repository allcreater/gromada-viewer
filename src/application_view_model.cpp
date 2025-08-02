module;
#include <flecs.h>
#include <imgui.h>
#include <sokol_app.h>

export module application.view_model;

import std;
import imgui_utils;
import Gromada.DataExporters;

import application.model;
import :map;
import :map_selector;
import :vids_window;

export class ViewModel {
public:
	explicit ViewModel(Model& model)
		: m_model{model} {
	}

	void updateUI() {
		drawMenu();
		const auto* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		//ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::Begin("Root window", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);
		
		m_mapViewModel.updateUI();

		ImGui::SetNextWindowPos({10, 50}, ImGuiCond_Appearing);
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

		    if (ImGui::BeginTabItem("Help")) {
				ImGui::TextUnformatted(
					R"(
Controls:
- Ctrl+Wheel - zoom in/out
- Right mouse button / Ctrl + mouse - move camera
- Left mouse button - select object
)");
		        ImGui::EndTabItem();
		    }

			ImGui::EndTabBar();
		}
		ImGui::End();

		ImGui::End();

		//ImGui::ShowDemoWindow();
	}


	void drawMenu() {
		constexpr const char* ExportPopup = "Export map JSON";
		const char* openPopup = nullptr;

		ImGui::BeginMainMenuBar();
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Export map JSON")) {
				openPopup = ExportPopup;
				m_savePopupfilenameBuffer.emplace();
			    if (const auto* activeMapPath = m_model.component<ActiveLevel>().get<Path>()) {
			        std::sprintf(m_savePopupfilenameBuffer->data(), "%s.json", activeMapPath->stem().u8string().c_str());
			    }
			}

			// TODO: reuse popup from previous item
			if (ImGui::MenuItem("Export vids to CSV")) {
				std::ofstream stream{"vids.csv", std::ios_base::out};
				stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

				const auto vids = m_model.get<const GameResources>()->vids();
				ExportVidsToCsv(vids, stream);
			}

			if (ImGui::MenuItem("Exit")) {
				sapp_request_quit();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Map")) {
		    m_mapViewModel.onMenu();
		    ImGui::EndMenu();
		}

		if (openPopup != nullptr) {
			ImGui::OpenPopup(openPopup);
		}

		if (ImGui::BeginPopupModal(ExportPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
			assert(m_savePopupfilenameBuffer.has_value());
			ImGui::InputText("Exporting map to", m_savePopupfilenameBuffer->data(), m_savePopupfilenameBuffer->size());
			if (ImGui::Button("OK", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();

				std::ofstream stream{m_savePopupfilenameBuffer->data(), std::ios_base::out};
				ExportMapToJson(m_model.saveMap(), stream);
			}
			ImGui::EndPopup();
		}

		ImGui::EndMainMenuBar();
	}

private:
	Model& m_model;

	std::optional<std::array<char, 256>> m_savePopupfilenameBuffer;

	VidsWindowViewModel m_vidsViewModel{m_model};
	MapViewModel m_mapViewModel{m_model};
	MapsSelectorViewModel m_mapsSelectorViewModel{m_model};
};