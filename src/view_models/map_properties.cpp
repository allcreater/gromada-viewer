module;
#include <flecs.h>
#include <imgui.h>

export module application.view_model : map_properties;

import std;

import application.model;
import Gromada.Map;
import imgui_utils;

export class MapPropertiesViewModel {
public:
    explicit MapPropertiesViewModel(flecs::world& model)
        : m_model{model} {}

    void updateUI() {
        const auto activeLevel = m_model.component<ActiveLevel>();
        auto & header = activeLevel.ensure<MapHeaderRawData>();


        MyImUtils::Text("Size: {} x {}", header.width, header.height);
        MyImUtils::Text("Observer pos: {}, {}", header.observerX, header.observerY);
        MyImUtils::Text("Scale: {}/{}", header.scaleX, header.scaleY);
        MyImUtils::Text("Start timer: {}", header.startTimer);
    }

private:
    flecs::world& m_model;
};