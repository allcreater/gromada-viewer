module;
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

export module application.dialogs;

import std;
import imgui_utils;

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
        m_selectedTile = std::min<int>(m_selectedTile, baseTiles.size() );

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
    int m_width = 30;
    int m_height = 20;
    int m_selectedTile = 1;
};

// Save-to-file dialog with a confirm-overwrite step and an error step, so a callback that throws
// (bad path, write failure, whatever the wrapped export/save function decides to throw) surfaces to
// the user instead of failing silently. All three steps reuse a single popup ID/window - switching
// on m_step - rather than nesting popups, which avoids having to juggle multi-level CloseCurrentPopup.
export class SaveDialog {
public:
    SaveDialog(std::function<void(std::ostream&&)> saveCallback, const char* windowTitle)
    : m_saveCallback{std::move(saveCallback)}
    , m_windowTitle{windowTitle}
    {}

    void open(std::string initialPath) {
        m_shouldOpen = true;
        m_currentState = State::PathEditing;
        m_filename = std::move(initialPath);
        m_errorMessage.clear();
    }

    void updateUI() {
        if (!MyImUtils::BeginModalPopup(m_windowTitle, m_shouldOpen))
            return;

        switch (m_currentState) {
        case State::PathEditing:
            ImGui::InputText("Save as", &m_filename);
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                if (std::filesystem::exists(m_filename))
                    m_currentState = State::ConfirmOverwrite;
                else
                    trySave();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            break;

        case State::ConfirmOverwrite:
            ImGui::TextWrapped("File \"%s\" already exists. Overwrite it?", m_filename.c_str());
            if (ImGui::Button("Overwrite", ImVec2(120, 0))) {
                trySave();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_currentState = State::PathEditing;
            }
            break;

        case State::Error:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            ImGui::TextWrapped("%s", m_errorMessage.c_str());
            ImGui::PopStyleColor();

            if (ImGui::Button("OK", ImVec2(120, 0))) {
                m_currentState = State::PathEditing;
            }
            break;
        }

        ImGui::EndPopup();
    }

private:
    void trySave() {
        try {
            std::ofstream stream{m_filename, std::ios_base::out | std::ios_base::binary};
            if (!stream)
                throw std::runtime_error(std::format("Failed to open \"{}\" for writing", m_filename));
            stream.exceptions(std::ofstream::failbit | std::ofstream::badbit);

            m_saveCallback(std::move(stream));

            ImGui::CloseCurrentPopup();
            m_currentState = State::PathEditing;
        } catch (const std::exception& e) {
            m_errorMessage = e.what();
            m_currentState = State::Error;
        }
    }

    enum class State { PathEditing, ConfirmOverwrite, Error };

    bool m_shouldOpen = false;
    State m_currentState = State::PathEditing;
    const char* m_windowTitle = "";
    std::string m_filename;
    std::string m_errorMessage;
    std::function<void(std::ostream&&)> m_saveCallback;
};

export class MessageDialog {
public:
    explicit MessageDialog(const char* windowTitle) : m_windowTitle{windowTitle} {}

    void open(std::string message) {
        m_shouldOpen = true;
        m_message = std::move(message);
    }

    void updateUI() {
        if (!MyImUtils::BeginModalPopup(m_windowTitle, m_shouldOpen))
            return;

        ImGui::TextUnformatted(m_message.c_str());
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

private:
    bool m_shouldOpen = false;
    const char* m_windowTitle = "";
    std::string m_message;
};