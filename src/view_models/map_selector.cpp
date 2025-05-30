module;
#include <imgui.h>
#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_glue.h>
#include <util/sokol_imgui.h>

export module application.view_model:map_selector;

import std;
import sokol.helpers;
import imgui_utils;

import application.model;

export class MapsSelectorViewModel {
public:
	explicit MapsSelectorViewModel(Model& model)
		: m_model{model}, m_mapsBaseDirectory{model.get<const GameResources>()->mapsPath()} {}

	void updateUI() {
		auto pathToCStr = [this, currentStr = std::u8string{}](
							  const MapEntry& mapEntry) mutable { return reinterpret_cast<const char*>(mapEntry.name.c_str()); };

	    const auto activeLevel = m_model.component<ActiveLevel>();

		if (MyImUtils::ListBox("Maps", &m_selectedMap, std::span{m_maps}, std::move(pathToCStr))) {
			const auto& selectedMap = m_maps[m_selectedMap];
			if (auto* currentPath = activeLevel.get<Path>(); !currentPath || (selectedMap.path != *currentPath)) {
				m_model.loadMap(selectedMap.path);
			}
		}
	}

private:
	Model& m_model;
	int m_selectedMap = 0;

	struct MapEntry {
		std::u8string name;
		std::filesystem::path path;
	};

	static std::vector<MapEntry> getMaps(const Model& model, const std::filesystem::path& mapsDirectory) {
		return std::filesystem::recursive_directory_iterator{mapsDirectory} | std::views::transform([&mapsDirectory](const auto& entry) {
			return MapEntry{std::filesystem::relative(entry.path(), mapsDirectory).u8string(), entry.path()};
		}) | std::views::filter([](const auto& entry) { return entry.path.extension() == ".map"; }) |
			   std::ranges::to<std::vector>();
	}


	std::filesystem::path m_mapsBaseDirectory;
	std::vector<MapEntry> m_maps = getMaps(m_model, m_mapsBaseDirectory);
};