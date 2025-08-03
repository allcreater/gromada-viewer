module;
#include <flecs.h>

export module application.model;

import std;
import utils;

import Gromada.ResourceReader;
import Gromada.Map;
export import Gromada.GameResources;

export import engine.world_components;

//TODO: move
export struct ObjectToPlaceMessage {
	std::uint16_t nvid;
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
		const auto map = ::loadMap(vids, path);
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
	    auto query = this->query_builder<const GameObject>().with(flecs::ChildOf).second<ActiveLevel>().build();

		Map map{};
		map.header = *activeLevel.get<MapHeaderRawData>();
		map.objects.reserve(query.count());
	    query.each([&map](const GameObject& obj) {
			map.objects.push_back(obj);
		});
		return map;
	}

private:
	flecs::world create_world(GameResources&& resources) {
		flecs::world world{};
        world.import<WorldModule>();
        world.import<EditorComponents>();

        world.emplace<GameResources>(std::move(resources));

        return world;
    }

};