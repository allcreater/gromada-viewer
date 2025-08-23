module;
#include <flecs.h>
#include <imgui.h>

export module application.view_model : map_properties;

import std;

import application.model;
import Gromada.Map;

export class MapPropertiesViewModel {
public:
    explicit MapPropertiesViewModel(flecs::world& model)
        : m_model{model} {}

    void updateUI() {
        const auto activeLevel = m_model.component<ActiveLevel>();
        auto & header = activeLevel.ensure<MapHeaderRawData>();


        ImGui::Text("Size: %d x %d", header.width, header.height);
        ImGui::Text("Observer pos: %d, %d", header.observerX, header.observerY);
        ImGui::Text("Scale: %u/%u", header.scaleX, header.scaleY);
        ImGui::Text("Start timer: %u", header.startTimer);
    }

private:
    flecs::world& m_model;
};