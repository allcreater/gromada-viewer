module;
#include <flecs.h>
#include <imgui.h>

export module application.view_model:sounds_window;

import std;
import application.model;
import engine.audio;
import imgui_utils;

export class SoundsWindowViewModel {
public:
    explicit SoundsWindowViewModel(Model& model)
        : m_model{model} {
    }

	void updateUI() {
		static ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg;

		const auto sounds = m_model.get<GameResources>().sounds();

		if (ImGui::BeginTable("sounds list", 4, flags)) {
			ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 30.0f);
			ImGui::TableSetupColumn("Channels", ImGuiTableColumnFlags_WidthStretch, 50.0f);
			ImGui::TableSetupColumn("Sample Rate", ImGuiTableColumnFlags_WidthStretch, 70.0f);
			ImGui::TableSetupColumn("Duration / Format", ImGuiTableColumnFlags_WidthStretch, 70.0f);
			ImGui::TableHeadersRow();

			for (std::size_t i = 0; i < sounds.size(); ++i) {
				const auto& sound = sounds[i];

				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::PushID(static_cast<int>(i));
				if (ImGui::Selectable("", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
					m_model.get_mut<AudioEngine>().playSound(sound);
				}
				ImGui::PopID();

				ImGui::SameLine();
				MyImUtils::Text("{}", i);

				ImGui::TableNextColumn();
				MyImUtils::Text("{}", sound.numChannels);

				ImGui::TableNextColumn();
				MyImUtils::Text("{}", sound.sampleRate);

				ImGui::TableNextColumn();
				std::visit([&sound](auto&& arg) {
					using T = std::decay_t<decltype(arg)>::value_type;
					float durationSeconds = static_cast<float>(arg.size()) / (sound.numChannels * sound.sampleRate);
					if constexpr (std::is_same_v<T, float>) {
					    MyImUtils::Text("[f32] {:.2f}s", durationSeconds);
					} else if constexpr (std::is_same_v<T, std::int16_t>) {
					    MyImUtils::Text("[i16] {:.2f}s", durationSeconds);
					} else if constexpr (std::is_same_v<T, std::uint8_t>) {
					    MyImUtils::Text("[u8] {:.2f}s", durationSeconds);
					}
				}, sound.waveData);
			}

			ImGui::EndTable();
		}
	}

private:
    Model& m_model;
};
