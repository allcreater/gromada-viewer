module;
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
		: m_model{model} {}

	void updateUI() {
		m_mapViewModel.drawMap();
		drawMenu();

		if (m_showVidsWindow) {
			if (!m_vidsViewModel) {
				m_vidsViewModel.emplace(m_model);
			}

			if (m_vidsViewModel) {
				m_vidsViewModel->updateUI();
			}
		}

		if (m_showMapsSelector) {
			m_mapsSelectorViewModel.update();
		}
	}


	void drawMenu() {
		constexpr const char* ExportPopup = "Export map JSON";
		const char* openPopup = nullptr;

		ImGui::BeginMainMenuBar();
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Export map JSON")) {
				openPopup = ExportPopup;
				m_savePopupfilenameBuffer.emplace();
				std::sprintf(m_savePopupfilenameBuffer->data(), "%s.json", m_model.map().filename().stem().u8string().c_str());
			}

			// TODO: reuse popup from previous item
			if (ImGui::MenuItem("Export vids to CSV")) {
				std::ofstream stream{"vids.csv", std::ios_base::out};
				stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
				ExportVidsToCsv(m_model.vids(), stream);
			}

			if (ImGui::MenuItem("Exit")) {
				sapp_request_quit();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Windows")) {
			ImGui::MenuItem("Vids", "ALT+V", &m_showVidsWindow);
			ImGui::MenuItem("Maps", "ALT+M", &m_showMapsSelector);

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View")) {
			if (ImGui::MenuItem("Zoom in", "CTRL+UP")) {
				m_mapViewModel.magnificationFactor++;
			}
			if (ImGui::MenuItem("Zoom out", "CTRL+DOWN")) {
				m_mapViewModel.magnificationFactor--;
			}
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
				ExportMapToJson(m_model.map(), stream);
			}
			ImGui::EndPopup();
		}

		ImGui::EndMainMenuBar();
	}

private:
	Model& m_model;

	bool m_showVidsWindow = false;
	bool m_showMapsSelector = true;
	std::optional<std::array<char, 256>> m_savePopupfilenameBuffer;

	std::optional<VidsWindowViewModel> m_vidsViewModel;
	MapViewModel m_mapViewModel{m_model};
	MapsSelectorViewModel m_mapsSelectorViewModel{m_model};
};