module;
#include <imgui.h>

export module application.view_model:map_selector;

import std;
import sokol;
import sokol.imgui;
import imgui_utils;

import application.model;

export class MapsSelectorViewModel {
public:
	explicit MapsSelectorViewModel(Model& model)
		: m_model{model}, m_mapsBaseDirectory{model.gamePath() / "maps"} {}

	void update() {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(300, 500));
		ImGui::Begin("Maps");

		auto pathToCStr = [this, currentStr = std::u8string{}](const auto& path) mutable {
			currentStr = std::filesystem::relative(path, m_mapsBaseDirectory).u8string();
			return reinterpret_cast<const char*>(currentStr.c_str());
		};

		if (ListBox("Maps", &m_selectedMap, std::span{m_maps}, std::move(pathToCStr))) {
			const auto& selectedMap = m_maps[m_selectedMap];
			if (selectedMap != m_model.map().filename()) {
				m_model.loadMap(selectedMap);
			}
		}
		ImGui::End();
		ImGui::PopStyleVar();
	}

	static std::vector<std::filesystem::path> getMaps(const Model& model, const std::filesystem::path& mapsDirectory) {
		return std::filesystem::recursive_directory_iterator{mapsDirectory} | std::views::transform([](const auto& entry) { return entry.path(); }) |
			   std::views::filter([](const auto& path) { return path.extension() == ".map"; }) | std::ranges::to<std::vector>();
	}

private:
	Model& m_model;
	int m_selectedMap = 0;

	std::filesystem::path m_mapsBaseDirectory;
	std::vector<std::filesystem::path> m_maps = getMaps(m_model, m_mapsBaseDirectory);
};