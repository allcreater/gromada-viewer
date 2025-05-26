module;
#include <flecs.h>

export module application.model;

import std;
import utils;

import Gromada.ResourceReader;
export import Gromada.GameResources;

export import engine.world_components;

//TODO: move
export struct ObjectToPlaceMessage {
	unsigned int nvid;
	std::uint8_t direction;
};


export struct Selected {};
export struct Selectable {};
export using Path = std::filesystem::path;

export struct EditorComponents {
    EditorComponents(flecs::world& world) {
        world.component<Selected>();
        world.component<Selectable>();
        world.component<Path>();
    }
};

export class Model : public flecs::world {
public:
	explicit Model(std::filesystem::path path)
		: flecs::world{create_world(GameResources{path})} {}

	void loadMap(const std::filesystem::path& path) {
		GromadaResourceReader mapReader{path};
		GromadaResourceNavigator mapNavigator{mapReader};

        auto& m_world = *this;

		const auto& vids = m_world.get<const GameResources>()->vids();
		const auto map = Map::load(vids, mapReader, mapNavigator);
	    const auto activeLevel = m_world.component<ActiveLevel>();
	    m_world.delete_with(flecs::ChildOf, activeLevel);

	    for (const auto& obj : map.objects) {
	        m_world.entity()
                .set<GameObject>(obj)
                .child_of(activeLevel);
	    }

	    activeLevel.set<MapHeaderRawData>(map.header);
        activeLevel.set<Path>(std::move(path));
	}

private:
	flecs::world create_world(GameResources&& resources) {
		flecs::world world{};
        world.import<World>();
        world.import<EditorComponents>();

        world.emplace<GameResources>(std::move(resources));

        return world;
    }

};