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

    // TODO: "this->" leaved to remember that it will be a free function soon
	void loadMap(std::filesystem::path path) {
		const auto& vids = this->get<const GameResources>()->vids();
		const auto map = Map::load(vids, path);
	    const auto activeLevel = this->component<ActiveLevel>();
	    this->delete_with(flecs::ChildOf, activeLevel);

	    for (const auto& obj : map.objects) {
	        this->entity()
                .set<GameObject>(obj)
                .child_of(activeLevel);
	    }

	    activeLevel.set<MapHeaderRawData>(map.header);
        activeLevel.set<Path>(std::move(path));
	}

	Map saveMap() const {
		const auto activeLevel = this->component<ActiveLevel>();

		Map map{};
		map.header = *activeLevel.get<MapHeaderRawData>();
		map.objects.reserve(this->count(flecs::ChildOf, activeLevel));
		this->each<GameObject>([&map, &activeLevel](flecs::entity e, const GameObject& obj) {
			if (e.has(flecs::ChildOf, activeLevel)) {
				map.objects.push_back(obj);
			}
		});
		return map;
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