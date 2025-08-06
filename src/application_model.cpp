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


export struct StaticLoaderSpecificAttributes {
    std::uint32_t original_id;
    std::uint32_t original_index;
};
export struct Selected {};
export struct Selectable {};
export using Path = std::filesystem::path;
export using Armies = std::array<Army, 2>;

export struct EditorComponents {
    EditorComponents(flecs::world& world) {
        world.component<StaticLoaderSpecificAttributes>();
        world.component<Selected>();
        world.component<Selectable>();
        world.component<Path>();
        world.component<Armies>();
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
	            .set<StaticLoaderSpecificAttributes>({.original_id = obj.id, .original_index = static_cast<std::uint16_t>(&obj - map.objects.data())})
                .child_of(activeLevel);
	    }

	    activeLevel.set<MapHeaderRawData>(map.header);
        activeLevel.set<Path>(std::move(path));
	    activeLevel.set<Armies>(std::move(map.armies));
	}

	Map saveMap() {
		const auto activeLevel = this->component<ActiveLevel>();
	    auto query = this->query_builder<const GameObject>().with(flecs::ChildOf).second<ActiveLevel>().build();

	    // NOTE: this pretty complex algorithm need just to minimize binary differences between original map and saved map
	    std::vector<GameObject> objects;
		std::vector<std::pair<const GameObject*, const StaticLoaderSpecificAttributes*>> delayedObjects;
	    std::set<std::uint32_t> knownIds;
		objects.reserve(query.count());

		query.each([&](flecs::entity entity, const GameObject& obj) {
	        if (const auto* existing_object_attribs = entity.get<StaticLoaderSpecificAttributes>()) {
	            delayedObjects.push_back({&obj, existing_object_attribs});
	            knownIds.insert(obj.id);
	        } else {
	            objects.push_back(obj);
	        }
		});

	    // generate IDs for objects that were
	    std::size_t numOfTries = 0;
	    std::ranges::for_each(objects, [&knownIds, rng = std::mt19937{std::random_device{}()}, &numOfTries](std::uint32_t& id) mutable {
            std::uniform_int_distribution<std::uint32_t> dist{1, std::numeric_limits<std::uint32_t>::max()};
	        while (!knownIds.insert(id = dist(rng)).second) { numOfTries++;}
	    }, &GameObject::id);

	    // NOTE: now it's time to restore original object positions and IDs
	    constexpr auto index_projection = [](const auto& pair) { return pair.second->original_index; };
	    std::ranges::sort(delayedObjects, {}, index_projection);
	    assert(std::ranges::unique(delayedObjects, {}, index_projection).begin() == delayedObjects.end());

        // NOTE: this is a bit complex, but it allows to minimize binary differences between original map and saved map
	    for (int index = objects.size(); const auto [object, attribs] : delayedObjects) {
	        assert(attribs->original_index <= objects.size());
	        auto& currentObject = objects.emplace_back(*object);
	        assert(currentObject.id == attribs->original_id);
	        currentObject.id = attribs->original_id; // restore original ID

	        if (attribs->original_index != (objects.size()-1)) {
	            std::swap(objects[attribs->original_index], currentObject);
	        }
        }
	    assert(activeLevel.get<MapHeaderRawData>() && activeLevel.get<Armies>());
		return Map {
		    .header = *activeLevel.get<MapHeaderRawData>(),
		    .objects = std::move(objects),
		    .armies = *activeLevel.get<Armies>(),
		};
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