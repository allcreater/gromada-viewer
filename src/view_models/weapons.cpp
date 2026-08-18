module;
#include <flecs.h>
#include <imgui.h>

export module application.view_model:weapons_window;

import std;
import application.model;
import utils;

export class WeaponsWindowViewModel {
public:
    explicit WeaponsWindowViewModel(Model& model)
        : m_model{model} {
    }

	void updateUI() {
		static ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg;

		const auto weapons = m_model.get<GameResources>().weapons();

		if (ImGui::BeginTable("weapons list", 7, flags)) {
			ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 30.0f);
			ImGui::TableSetupColumn("Unit type", ImGuiTableColumnFlags_WidthStretch, 50.0f);
			ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthStretch, 50.0f);
			ImGui::TableSetupColumn("Always zero", ImGuiTableColumnFlags_WidthStretch, 50.0f);
			ImGui::TableSetupColumn("Weapon range", ImGuiTableColumnFlags_WidthStretch, 50.0f);
			ImGui::TableSetupColumn("Scatter", ImGuiTableColumnFlags_WidthStretch, 50.0f);
			ImGui::TableSetupColumn("Cooldown", ImGuiTableColumnFlags_WidthStretch, 50.0f);
			ImGui::TableHeadersRow();

			for (std::size_t i = 0; i < weapons.size(); ++i) {
				const auto& weapon = weapons[i];

				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("%zu", i);

				ImGui::TableNextColumn();
				ImGui::Text("%s", to_string(Flags{weapon.targetCategory}).c_str());

				ImGui::TableNextColumn();
				ImGui::Text("%u", weapon.flags);

				ImGui::TableNextColumn();
				ImGui::Text("%u", weapon.alwaysZero);

				ImGui::TableNextColumn();
				ImGui::Text("%u", weapon.weaponRange);

				ImGui::TableNextColumn();
				ImGui::Text("%u", weapon.scatter);

				ImGui::TableNextColumn();
				ImGui::Text("%u", weapon.cooldown);
			}

			ImGui::EndTable();
		}
	}

private:
    Model& m_model;
};
