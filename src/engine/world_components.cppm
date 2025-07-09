module;
#include <flecs.h>

export module engine.world_components;

import std;
import Gromada.GameResources;
import Gromada.VisualLogic;
import engine.objects_view;

export {

struct DestroyAfterUpdate {};
struct ActiveLevel {};
struct AnimationComponent {
    Action action = Action::act_stand;
    std::uint32_t frame_phase = 0;
    float next_frame_delay = 0.0f;
    std::uint32_t current_frame = 0;
};

class World {
public:
	World(flecs::world& world) {
		world.component<GameObject>();
		world.component<Vid>();
		world.component<MapHeaderRawData>();
		world.component<ObjectsView>();
		world.component<GameResources>();
		world.component<DestroyAfterUpdate>();
		world.component<AnimationComponent>();
		world.component<ActiveLevel>().add(flecs::Exclusive);

	    world.emplace<ObjectsView>(world);

        world.add<ActiveLevel>();

	    world.observer<const GameResources, const GameObject>()
	        .term_at(0).singleton().filter()
	        .event(flecs::OnSet)
			.each([](flecs::entity entity, const GameResources& res, const GameObject& object) {
	            entity.emplace<Vid>(res.vids()[object.nvid]);
			    entity.emplace<AnimationComponent>(AnimationComponent{.frame_phase = static_cast<std::uint32_t>(std::hash<std::uint64_t>{}(entity.id()))});
	        });

	    world.system<DestroyAfterUpdate>().kind(flecs::OnStore).each([](flecs::entity entity, DestroyAfterUpdate) {
	        entity.destruct();
	    });

	    world.system<AnimationComponent, const Vid, const GameObject>()
	        .kind(flecs::OnUpdate)
	        .each([](flecs::iter& it, size_t, AnimationComponent& animation, const Vid& vid, const GameObject& object) {
	            animation.next_frame_delay -=  it.delta_time() * 1000.0f; //ms
	            if (animation.next_frame_delay < 0.0f) {
	                int increment = std::ceil(-animation.next_frame_delay / vid.graphics().frameDuration);
	                animation.next_frame_delay +=  increment * vid.graphics().frameDuration;
	                assert(animation.next_frame_delay >= 0.0f);
	                animation.frame_phase += increment;
	            }

	            auto [minIndex, maxIndex] = getAnimationFrameRange(vid, animation.action, object.direction);
	            animation.current_frame = animation.frame_phase % (maxIndex - minIndex + 1) + minIndex;
	            assert(animation.current_frame <= vid.graphics().numOfFrames);
	    });
	}
};
}