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
        const auto& activeLevel = m_model.component<ActiveLevel>();
        const auto& header = activeLevel.ensure<MapHeaderRawData>();

        MyImUtils::Text("Size: {} x {}", header.width, header.height);
        MyImUtils::Text("Observer pos: {}, {}", header.observerX, header.observerY);
        MyImUtils::Text("Scale: {}/{}", header.scaleX, header.scaleY);
        MyImUtils::Text("Start timer: {}", header.startTimer);

        if (ImGui::CollapsingHeader("Armies")) {
            const auto& armies = activeLevel.ensure<Armies>();

            constexpr ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV;
            if (ImGui::BeginTable("Armies", 2, flags)) {
                ImGui::TableSetupColumn("Army 0");
                ImGui::TableSetupColumn("Army 1");
                ImGui::TableHeadersRow();
                ImGui::TableNextRow();

                for (const auto& army : armies) {
                    ImGui::TableNextColumn();

                    MyImUtils::Text("a/b/c: {}/{}/{}", army.a, army.b, army.c);
                    MyImUtils::Text("Flagman id: {}", army.flagman_id);

                    ImGui::Spacing();
                    ImGui::TextUnformatted("Squads:");
                    for (std::size_t squadIndex = 0; const auto& squad : army.squads) { // NOTE: can't use std::views::enumerate until it will be supported by libc++
                        const auto members = squad
                            | std::views::transform([](std::uint32_t id) { return std::to_string(id); })
                            | std::views::join_with(std::string_view{", "})
                            | std::ranges::to<std::string>();

                        MyImUtils::Text("  {}: [{}]", squadIndex++, members);
                    }
                }

                ImGui::EndTable();
            }
        }

        ImGui::NewLine();
        ImGui::TextWrapped( "Note: this is read-only visualisation of raw data from loaded map.\nThe map size and squads will be written automatically on map save.\nSome params are just fixed, some seems not important.\nPurpose of some parameters is unknown yet");
    }

private:
    flecs::world& m_model;
};