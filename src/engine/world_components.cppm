module;
#include <flecs.h>

export module engine.world_components;

import std;
import Gromada.GameResources;
import engine.objects_view;

export {

struct DestroyAfterUpdate {};
struct ActiveLevel {};

class World {
public:
	World(flecs::world& world) {
		world.component<GameObject>();
		world.component<VidComponent>();
		world.component<MapHeaderRawData>();
		world.component<ObjectsView>();
		world.component<GameResources>();
		world.component<DestroyAfterUpdate>();
		world.component<ActiveLevel>().add(flecs::Exclusive);

	    world.emplace<ObjectsView>(world);

        world.add<ActiveLevel>();

	    world.observer<const GameObject>()
	        .event(flecs::OnSet)
			.each([](flecs::entity entity, const GameObject& object) { // TODO: try param GameResources
			    const auto& res = *entity.world().get<const GameResources>();
	            const auto& vid = std::get<0>(res.getVid(object.nvid));
	            entity.emplace<VidComponent>(vid);
	        });

	    world.system<DestroyAfterUpdate>().kind(flecs::OnStore).each([](flecs::entity entity, DestroyAfterUpdate) {
	        entity.destruct();
	    });
	}
};
}