module;
#include <imgui.h>

export module application.view_model:vids_window;

import std;
import sokol;
import sokol.imgui;
import imgui_utils;

import application.model;

void VidRawData_ui(const VidRawData& self);

export class VidsWindowViewModel {
public:
	explicit VidsWindowViewModel(Model& model)
		: m_model{model} {}

	void updateUI() {
		// By default, if we don't enable ScrollX the sizing policy for each column is "Stretch"
		// All columns maintain a sizing weight, and they will occupy all available width.
		static ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
									   ImGuiTableFlags_ContextMenuInBody;

		ImGui::Begin("Vids");
		if (ImGui::BeginTable("table1", 2, flags)) {
			ImGui::TableNextColumn();

			ImVec2 listBoxSize{-FLT_MIN, ImGui::GetWindowHeight() - 80.0f};
			bool changed = true;
			{
				if (ImGui::BeginTable("vids_list_table", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter, listBoxSize)) {
					ImGui::TableSetupColumn("NVID", ImGuiTableColumnFlags_WidthFixed, 50.0f);
					ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
					for (auto [index, vid] : m_model.vids() | std::views::enumerate) {
						ImGui::TableNextColumn();

						if (ImGui::Selectable(std::to_string(index).c_str(), m_selectedSection == index, ImGuiSelectableFlags_SpanAllColumns)) {
							m_selectedSection = index;
						}
						ImGui::TableNextColumn();
						ImGui::Text("%s", vid.name.data());

						ImGui::TableNextRow();
					}

					ImGui::EndTable();
				}
			}

			if (m_selectedSection >= 0 && m_selectedSection < m_model.vids().size()) {
				ImGui::TableNextColumn();
				VidRawData_ui(m_model.vids()[m_selectedSection]);
			}
			ImGui::EndTable();

			if (ImGui::Button("Export CSV")) {
				m_model.write_csv("vids.csv");
			}
		}
		ImGui::End();
	}

private:
	Model& m_model;

	int m_selectedSection = 0;
};

void VidRawData_ui(const VidRawData& self) {
	ImGui::Text("%s", self.name.data());
	ImGui::Text("unitType: %i ", self.unitType);
	ImGui::Text("Behave: %i ", self.behave);
	ImGui::Text("Flags % i", self.flags);
	ImGui::Text("Collision mask: %x", self.collisionMask);
	ImGui::Text("W: %i", self.anotherWidth);
	ImGui::Text("H: %i", self.anotherHeight);
	ImGui::Text("Z: %i", self.z_or_height);
	ImGui::Text("max HP: %i", self.maxHP);
	ImGui::Text("grid radius: %i", self.gridRadius);
	ImGui::Text("???: %i", self.p6);
	ImGui::Text("Speed % i", self.speed);

	ImGui::Text("???: %i", self.hz1);
	ImGui::Text("???: %i", self.hz2);
	ImGui::Text("???: %i", self.hz3);
	ImGui::Text("Army: %i", self.army);
	ImGui::Text("Weapon?: %i", self.someWeaponIndex);
	ImGui::Text("???: %i", self.hz4);
	ImGui::Text("Death size margin: % i", self.deathSizeMargin);
	ImGui::Text("Death ??: %i", self.somethingAboutDeath);
	ImGui::Text("some X: %i", self.sX);
	ImGui::Text("some Y: %i", self.sY);
	ImGui::Text("some Z: %i", self.sZ);

	ImGui::Text("???: %i", self.hz5);
	ImGui::Text("???: %i", self.hz6);
	ImGui::Text("direction? % i", self.direction);
	ImGui::Text("z2 : % i", self.z);


	constexpr auto classifyVisualBehavior = [](std::uint8_t behavior) -> const char* {
		switch (behavior & 0x3F) {
		case 0:
			return "unpack mode 1";
		case 6:
			return "unpack mode 2";
		case 8:
			return "unpack mode 3";
		default:
			return "not packed";
		}
	};

	if (!self.vid) {
		ImGui::Text("Source nVid: %i", -self.dataSizeOrNvid);
		return;
	}

	ImGui::Text("frames size: %i", self.dataSizeOrNvid);
	ImGui::Text("Visual behavior: %x (%s)", self.visualBehavior, classifyVisualBehavior(self.visualBehavior));
	ImGui::Text("???: %i", self.hz7);
	ImGui::Text("numOfFrames: %i", self.numOfFrames);
	ImGui::Text("dataSize: %i", self.dataSize);
	ImGui::Text("imgWidth: %i", self.imgWidth);
	ImGui::Text("imgHeight: %i", self.imgHeight);
}