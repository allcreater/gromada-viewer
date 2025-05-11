module;

export module application.model;

import std;
import utils;

import Gromada.ResourceReader;
export import Gromada.GameResources;

export import engine.objects_view;

//TODO: move
export struct ObjectToPlaceMessage {
	unsigned int nvid;
	std::uint8_t direction;
};

export class Model {
public:
	explicit Model(std::filesystem::path path)
		: m_resources{path}, m_gamePath{path.parent_path()}, m_objectsView{m_resources, m_activeMap} {}

	const std::span<const Vid> vids() const { return m_resources.vids(); }
	
	// NOTE: Map is just a DTO type, for game logic a new class will be needed
	Map& map() { return m_activeMap; }
	const Map& map() const { return m_activeMap; }
	const std::filesystem::path& activeMapPath() const { return m_activeMapPath; }

	const ObjectsView& objectsView() const { return m_objectsView; }

	const std::filesystem::path& gamePath() const { return m_gamePath; }

	std::vector<std::size_t>& selectedMapObjects() { return m_selectedObjects;  }

	void update() { m_objectsView.update(); }

	void loadMap(const std::filesystem::path& path) {
		GromadaResourceReader mapReader{path};
		GromadaResourceNavigator mapNavigator{mapReader};

		m_activeMap = Map::load(vids(), mapReader, mapNavigator);
		m_activeMapPath = std::move(path);
		m_selectedObjects.clear();
	}

private:
	GameResources m_resources;
	Map m_activeMap;
	ObjectsView m_objectsView;
	std::vector<std::size_t> m_selectedObjects;

	std::filesystem::path m_activeMapPath;
	std::filesystem::path m_gamePath;
};