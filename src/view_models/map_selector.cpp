module;
#include <imgui.h>


export module application.view_model:map_selector;

import std;
import imgui_utils;

import application.model;
import application.dialogs;

export class MapsSelectorViewModel {
public:
	explicit MapsSelectorViewModel(Model& model)
		: m_model{model}
		, m_mapsBaseDirectory{model.get<const GameResources>().mapsPath()}
		, m_errorDialog{"Error"}
{}

	void updateUI() {
	    const auto activeLevel = m_model.component<ActiveLevel>();

		if (MyImUtils::ListBox("Maps", &m_selectedMap, std::span<const MapEntry>{m_maps}, MyImUtils::MakeSelectableCallback<const MapEntry&>(&MapEntry::name))) {
			const auto& selectedMap = m_maps[m_selectedMap];
			if (auto* currentPath = activeLevel.try_get<Path>(); !currentPath || (selectedMap.path != *currentPath)) {
				try {
					m_model.loadMap(selectedMap.path);
				} catch ( const std::exception& e) {
					m_errorDialog.open(std::format("Error loading map \"{}\":\n\n{}", selectedMap.path.generic_string(), e.what()));
				}
			}
		}

		m_errorDialog.updateUI();
	}

private:
	Model& m_model;
	int m_selectedMap = 0;
	MessageDialog m_errorDialog;

	struct MapEntry {
		std::u8string name;
		std::filesystem::path path;
	};

	static std::vector<MapEntry> getMaps(const Model& model, const std::filesystem::path& mapsDirectory) {
		if (!std::filesystem::exists(mapsDirectory))
			return {};

		return std::filesystem::recursive_directory_iterator{mapsDirectory} | std::views::transform([&mapsDirectory](const auto& entry) {
			return MapEntry{std::filesystem::relative(entry.path(), mapsDirectory).u8string(), entry.path()};
		}) | std::views::filter([](const auto& entry) { return entry.path.extension() == ".map"; }) |
			   std::ranges::to<std::vector>();
	}


	std::filesystem::path m_mapsBaseDirectory;
	std::vector<MapEntry> m_maps = getMaps(m_model, m_mapsBaseDirectory);
};