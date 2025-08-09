module;
#include <flecs.h>

export module engine.world_components;

import std;
import Gromada.GameResources;
import Gromada.Map;
import Gromada.VisualLogic;
export import engine.objects_view;

export {

    struct DestroyAfterUpdate {};
    struct ActiveLevel {};
    struct AnimationComponent {
        Action action = Action::act_stand;
        std::uint32_t frame_phase = 0;
        float next_frame_delay = 0.0f;
        std::uint32_t current_frame = 0;
    };

    class WorldModule {
    public:
        WorldModule(flecs::world& world) {
            world.component<GameObject::Payload>();
            world.component<VidComponent>();
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

            world.observer<const VidComponent>()
                .event(flecs::OnSet)
                .each([](flecs::entity entity, const VidComponent& vid) {
                if (vid->linkedObjectVid > 0) {
                    entity.world()
                        .entity()
                        .set<Transform, Local>({
                            .x = vid->linkX,
                            .y = vid->linkY,
                            .z = vid->linkZ,
                            .direction = 0,
                        })
                        .emplace<VidComponent>(vid.parent(), vid->linkedObjectVid)
                        .child_of(entity);
                }

                entity.emplace<AnimationComponent>(AnimationComponent{.frame_phase = static_cast<std::uint32_t>(std::hash<std::uint64_t>{}(entity.id()))});
                entity.add<Transform, World>();
            });


            world.system<DestroyAfterUpdate>().kind(flecs::PostFrame).each([](flecs::entity entity, DestroyAfterUpdate) { entity.destruct(); });

            world.system<AnimationComponent, const VidComponent, const Transform>()
                .kind(flecs::OnUpdate)
                .term_at(2).second<World>()
                .each([](flecs::iter& it, size_t, AnimationComponent& animation, const Vid& vid, const Transform& wt) {
                    animation.next_frame_delay -= it.delta_time() * 1000.0f; // ms
                    if (animation.next_frame_delay < 0.0f) {
                        int increment = std::ceil(-animation.next_frame_delay / vid.graphics().frameDuration);
                        animation.next_frame_delay += increment * vid.graphics().frameDuration;
                        assert(animation.next_frame_delay >= 0.0f);
                        animation.frame_phase += increment;
                    }

                    auto [minIndex, maxIndex] = getAnimationFrameRange(vid, animation.action, wt.direction);
                    animation.current_frame = animation.frame_phase % (maxIndex - minIndex + 1) + minIndex;
                    assert(animation.current_frame <= vid.graphics().numOfFrames);
                });

            world.system<const Transform, const Transform*, Transform>()
                .term_at(0).second<Local>()
                .term_at(1).second<World>() //.parent().cascade()
                .term_at(2).second<World>()
                .term_at(1).parent().cascade()
                .each([](const Transform& local, const Transform* parent_world, Transform& out_world) {
                    out_world = local;
                    if (parent_world) {
                        out_world.x += parent_world->x;
                        out_world.y += parent_world->y;
                        out_world.z += parent_world->z;
                        out_world.direction += parent_world->direction;
                    }
                });
        }
    };
}