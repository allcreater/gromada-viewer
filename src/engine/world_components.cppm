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

struct Local {};
struct Global {};
struct Transform {
    int x = 0, y = 0, z = 0;
    std::uint8_t direction = 0;
};

class World {
public:
	World(flecs::world& world) {
		world.component<GameObject>();
		world.component<Vid>(); // TODO: are we need a copy?
		world.component<MapHeaderRawData>();
		world.component<ObjectsView>();
		world.component<GameResources>();
		world.component<DestroyAfterUpdate>();
		world.component<AnimationComponent>();
		world.component<ActiveLevel>().add(flecs::Exclusive);

        world.component<Local>();
        world.component<World>();
        world.component<Transform>();

	    world.emplace<ObjectsView>(world);

        world.add<ActiveLevel>();

	    world.observer<const GameResources, const GameObject>()
	        .term_at(0).singleton().filter()
	        .event(flecs::OnSet)
			.each([](flecs::entity entity, const GameResources& res, const GameObject& object) {
                const auto& vid = res.vids()[object.nvid];

			    // doubtfully, but okay :)
			    // TODO: must be a better way to do this
                if (!entity.has<Vid>() && vid.linkedObjectVid > 0) {
                    entity.world().entity().set<GameObject>({
                        .nvid = vid.linkedObjectVid,
                        .x = vid.linkX,
                        .y = vid.linkY,
                        .z = vid.linkZ,
                        .direction = object.direction,
                    }).child_of(entity);
                }

	            entity.emplace<Vid>(vid);
			    entity.emplace<AnimationComponent>(AnimationComponent{.frame_phase = static_cast<std::uint32_t>(std::hash<std::uint64_t>{}(entity.id()))});

			    entity.set<Transform, Local>({.x = object.x, .y = object.y, .z = object.z, .direction = static_cast<std::uint8_t>(object.direction)});
			    entity.add<Transform, World>();
	        });


	    world.system<DestroyAfterUpdate>().kind(flecs::PostFrame).each([](flecs::entity entity, DestroyAfterUpdate) {
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

	    world.system<const Transform, const Transform*, Transform>()
	        .term_at(0).second<Local>()
	        .term_at(1).second<World>()//.parent().cascade()
	        .term_at(2).second<World>()
	        .term_at(1).parent().cascade()
	        .each([](const Transform& local, const Transform* parent, Transform& out) {
                out = local;
	            if (parent) {
	                out.x += parent->x;
	                out.y += parent->y;
                    out.z += parent->z;
	                out.direction += parent->direction;
	            }
	    });

	}
};
}