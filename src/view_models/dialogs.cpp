module;
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

export module application.view_model : dialogs;

import std;
import imgui_utils;
import Gromada.DataExporters;

import application.model;

export class NewMapDialog {
public:
    void open() {
        m_shouldOpen = true;
    }

    void updateUI(Model& model) {
        if (!MyImUtils::BeginModalPopup("New map", m_shouldOpen))
            return;

        ImGui::InputInt("Width", &m_width);
        ImGui::InputInt("Height", &m_height);

        auto& gameResources = model.get<GameResources>();
        auto baseTiles = gameResources.baseTilesVids();
        MyImUtils::ComboBox("Ground", &m_selectedTile, baseTiles, [](const auto& vid) {
            return vid ? vid->getName() : "None [size in pixels]";
        });

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            model.newMap(baseTiles[m_selectedTile], m_width, m_height);
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

private:
    bool m_shouldOpen = false;
    int m_width = 10;
    int m_height = 10;
    int m_selectedTile = 0;
};

export class ExportMapDialog {
public:
    void open(Model& model) {
        m_shouldOpen = true;

        if (const auto* activeMapPath = model.component<ActiveLevel>().try_get<Path>())
            m_filename = std::format("{}.json", activeMapPath->stem().generic_string());
        else
            m_filename.clear();
    }

    void updateUI(std::span<const Vid> vids, Model& model) {
        if (!MyImUtils::BeginModalPopup("Export map JSON", m_shouldOpen))
            return;

        ImGui::InputText("Exporting map to", &m_filename);
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();

            std::ofstream stream{m_filename, std::ios_base::out};
            ExportMapToJson(vids, model.saveMap(), stream);
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

private:
    bool m_shouldOpen = false;
    std::string m_filename;
};
