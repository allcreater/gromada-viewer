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

export class SaveDialog {
public:
    SaveDialog(std::function<void(std::ostream&&)> saveCallback, const char* windowTitle)
    : m_saveCallback{std::move(saveCallback)}
    , m_windowTitle{windowTitle}
    {}

    void open(std::string initialPath) {
        m_shouldOpen = true;

        m_filename = initialPath;
    }

    void updateUI() {
        if (!MyImUtils::BeginModalPopup(m_windowTitle, m_shouldOpen))
            return;

        ImGui::InputText("Save as", &m_filename);
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();

            try {
                std::ofstream stream{m_filename, std::ios_base::out};
                stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

                m_saveCallback(std::move(stream));
            } catch (std::ifstream::failure& e) {
                std::cerr << e.what() << std::endl;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

private:
    bool m_shouldOpen = false;
    const char* m_windowTitle = "";
    std::string m_filename;
    std::function<void(std::ostream&&)> m_saveCallback;
};
