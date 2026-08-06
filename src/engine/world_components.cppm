module;
#include <flecs.h>

export module engine.world_components;

import std;
export import Gromada.GameResources;
import Gromada.Map;
import Gromada.VisualLogic;
export import engine.objects_view;

export {
    struct DestroyAfterUpdate {};
    struct ActiveLevel {};
    struct AnimationComponent {
        Stopwatch stopwatch;
        Action action = Action::act_stand;
        std::uint32_t current_frame = 0;
    };

    class WorldModule {
    public:
        WorldModule(flecs::world& world) {
            world.component<GameObject::Payload>();
            world.component<VidRef>();
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

            world.observer<const VidRef>()
                .event(flecs::OnSet)
                .each([](flecs::entity entity, const VidRef& vid) {
                if (vid->linkedObjectVid > 0) {
                    entity.world()
                        .entity()
                        .set<Transform, Local>({
                            .x = vid->linkX,
                            .y = vid->linkY,
                            .z = vid->linkZ,
                            .direction = 0,
                        })
                        .emplace<VidRef>(vid.parent().getVid(vid->linkedObjectVid))
                        .child_of(entity);
                }

                entity.emplace<AnimationComponent>(AnimationComponent{
                    .current_frame = static_cast<std::uint32_t>(std::hash<std::uint64_t>{}(entity.id()))
                });
                entity.add<Transform, World>();
            });


            world.system<DestroyAfterUpdate>().kind(flecs::PostFrame).each([](flecs::entity entity, DestroyAfterUpdate) { entity.destruct(); });

            world.system<AnimationComponent, const VidRef, const Transform>()
                .kind(flecs::OnUpdate)
                .term_at(2).second<World>()
                .each([](flecs::iter& it, size_t, AnimationComponent& animation, const Vid& vid, const Transform& wt) {
                    animation.current_frame += animation.stopwatch.advance(it.delta_time(), vid.graphics().frameDuration * 0.001f);

                    const auto frame_range = getAnimationFrameRange(vid, animation.action, wt.direction);
                    if (frame_range) {
                        animation.current_frame = animation.current_frame % (frame_range->second - frame_range->first + 1) + frame_range->first;
                    } else {
                        animation.current_frame = 0;
                    }

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