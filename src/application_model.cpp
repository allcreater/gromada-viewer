module;
#include <flecs.h>

export module application.model;

import std;
import utils;

import Gromada.ResourceReader;
import Gromada.Map;
import engine.bounding_box;
import engine.level_renderer;

export import Gromada.GameResources;

export import engine.world_components;

//TODO: move
export struct ObjectToPlaceMessage {
	std::uint16_t nvid;
	std::uint8_t direction;
};


export struct EditorOrdering {
    std::uint32_t uid = 0;
    std::uint32_t index = 0;
};
export struct Selected {};
export struct ObjectPrototype {};
export using Path = std::filesystem::path;
export using Armies = std::array<Army, 2>;

export struct EditorComponents {
    EditorComponents(flecs::world& world) {
        world.component<EditorOrdering>();
        world.component<Selected>();
		world.component<ObjectPrototype>();
		world.component<Path>();
		world.component<Armies>();
	}
};

export class Model : public flecs::world {
public:
	explicit Model(std::filesystem::path path)
		: flecs::world{create_world(GameResources{path})} {}

    void newMap() {
	    const auto activeLevel = this->component<ActiveLevel>();
	    this->delete_with(flecs::ChildOf, activeLevel);

		activeLevel.set<MapHeaderRawData>({});
		activeLevel.set<Path>({});
	    activeLevel.set<Armies>({});
	}

    // TODO: "this->" leaved to remember that it will be a free function soon
	void loadMap(std::filesystem::path path) {
		const auto* gameResources = this->get<const GameResources>();
		const auto map = ::loadMap(gameResources->vids(), path);
	    const auto activeLevel = this->component<ActiveLevel>();
	    this->delete_with(flecs::ChildOf, activeLevel);

	    for (const auto& obj : map.objects) {
	        this->entity()
                .emplace<VidComponent>(*gameResources, obj.nvid)
	            .set<Transform, Local>({.x = obj.x, .y = obj.y, .z = obj.z, .direction = obj.direction})
	            .set<GameObject::Payload>(obj.payload)
	            .set<EditorOrdering>({.uid = obj.id, .index = static_cast<std::uint16_t>(&obj - map.objects.data())})
                .child_of(activeLevel);
	    }

	    activeLevel.set<MapHeaderRawData>(map.header);
        activeLevel.set<Path>(std::move(path));
	    activeLevel.set<Armies>(std::move(map.armies));
	}

    static GameObject makeGameObject(const VidComponent& vid, const Transform& transform, const GameObject::Payload* payload, std::uint32_t id) {
	    assert(transform.x > std::numeric_limits<std::int16_t>::min() && transform.y > std::numeric_limits<std::int16_t>::min() && transform.z > std::numeric_limits<std::int16_t>::min());
	    assert(transform.x < std::numeric_limits<std::int16_t>::max() && transform.y < std::numeric_limits<std::int16_t>::max() && transform.z < std::numeric_limits<std::int16_t>::max());

        return GameObject {
            .nvid = vid.nvid(),
            .x = static_cast<std::int16_t>(transform.x),
            .y = static_cast<std::int16_t>(transform.y),
            .z = static_cast<std::int16_t>(transform.z),
            .direction = transform.direction,
            .action = std::to_underlying(Action::act_stand), // TODO: save real action
            .payload = payload ? *payload : GameObject::Payload{},
            .id = id,
        };
    }

    // this function is so complex to reduce the binary differences between the original and saved map.
    // It tries to save original objects on the same position and with the same ID
    std::vector<flecs::entity> prepareObjectsToExport() {
	    auto query = this->query_builder<const VidComponent, const Transform>()
	        .term_at(1).second<Local>() // Or world? Anyway, should be the same for top-level objects
	        .with(flecs::ChildOf).second<ActiveLevel>()
	        .build();

	    std::unordered_set<std::uint32_t> known_ids;
	    std::deque<flecs::entity> unordered_objects;
	    std::vector<flecs::entity> objects(query.count());

	    // First step - collect all known objects and try to place them in the correct order
	    query.each([&](flecs::entity entity, const VidComponent& vid, const Transform& transform) {
	        if (auto* existing_object_attribs = entity.get<EditorOrdering>()) {
	            const auto [id, index] = *existing_object_attribs;
	            if (index < objects.size()) {
	                assert(objects[index].id() == 0);
                    objects[index] = entity;
                } else {
                    unordered_objects.push_front(entity);
                }
	            known_ids.insert(id);
	        } else {
	            unordered_objects.push_back(entity);
	        }
	    });

        auto generate_new_id = [&known_ids, rng = std::mt19937{std::random_device{}()}] mutable {
            std::uniform_int_distribution<std::uint32_t> dist{1, std::numeric_limits<std::uint32_t>::max()};
            std::uint32_t id;
            do {
                id = dist(rng);
            } while (!known_ids.insert(id).second);
            return id;
        };

	    // Second step - fill in the gaps with new objects
	    auto free_indices = std::views::iota(std::size_t{0}, objects.size()) | std::views::filter([&objects](std::size_t index) {return objects[index].id() == 0;});
	    for (auto i : free_indices) {
	        objects[i] = unordered_objects.front();
	        {
	            auto& ordering = objects[i].ensure<EditorOrdering>();
	            ordering.index = static_cast<std::uint32_t>(i);
	            if (ordering.uid == 0)
	                ordering.uid = generate_new_id();
	        }
	        unordered_objects.pop_front();
	    }
	    assert(unordered_objects.empty());

	    return objects;
	}


    Map saveMap() {
	    const auto activeLevel = this->component<ActiveLevel>();
	    auto& header = activeLevel.ensure<MapHeaderRawData>();

        auto entities = prepareObjectsToExport();

	    // translate coordinates so that (0,0) is top-left corner of the map
        [&] {
            const auto map_bounds = std::reduce(entities.begin(), entities.end(), BoundingBox{}, [](BoundingBox bb, flecs::entity obj) {
                auto transform = obj.get<Transform, Local>();
                return bb.extend(transform->x, transform->y);
            });

            std::ranges::for_each(entities, [&map_bounds](flecs::entity obj) {
                auto transform = obj.get_mut<Transform, Local>();
                transform->x -= map_bounds.left;
                transform->y -= map_bounds.top;
            });

            header.height = map_bounds.height();
            header.width = map_bounds.width();
            header.observerX -= map_bounds.left;
            header.observerY -= map_bounds.top;
        }();

	    return Map {
	        .header = header,
			.objects = entities | std::views::transform([i = 0](const flecs::entity& entity) mutable {
				const auto& vid = *entity.get<VidComponent>();
				const auto& transform = *entity.get<Transform, Local>();
				const auto* payload = entity.get<GameObject::Payload>();
				const auto& ordering = *entity.get<EditorOrdering>();
				assert(ordering.index == i++);
				return makeGameObject(vid, transform, payload, ordering.uid);
			}) | std::ranges::to<std::vector<GameObject>>(),
            .armies = activeLevel.ensure<Armies>(),
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