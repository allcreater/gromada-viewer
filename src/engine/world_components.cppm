module;
#include <flecs.h>
#include <cassert>

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

    // Tag identifying an entity spawned as a map object's linked child (vid->linkedObjectVid), so
    // it can be told apart from other children during reconciliation.
    struct LinkedObject {};

    // Reconciliation stamp: the nvid the entity's derived state was last built from. An absent or
    // mismatched stamp means the entity needs a rebuild (see WorldModule's reconciliation).
    struct SyncedVid {
        std::uint16_t nvid;
    };

    // Enables/disables a map object together with its linked children. The single entry point
    // for hiding objects from the document (soft delete, undo/redo of one): plain enable()/
    // disable() on the object alone would leave its linked children rendering as ghosts.
    // Disabled objects are skipped by all queries, so they are invisible to rendering,
    // selection, region queries and export, but their handles stay valid - which is what lets
    // History restore them without ever minting new entity ids.
    void setMapObjectEnabled(flecs::entity entity, bool enabled) {
        if (enabled)
            entity.enable();
        else
            entity.disable();

        entity.children(flecs::ChildOf, [enabled](flecs::entity child) {
            if (!child.has<LinkedObject>())
                return;

            if (enabled)
                child.enable();
            else
                child.disable();
        });
    }

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
            world.component<LinkedObject>();
            world.component<SyncedVid>();
            world.component<ActiveLevel>().add(flecs::Exclusive);

            world.component<Local>();
            world.component<World>();
            world.component<Transform>();

            world.emplace<ObjectsView>(world);

            world.add<ActiveLevel>();

            // Matches all map objects; the SyncedVid term is optional, so entities without the stamp match too and report it as a null pointer.
            m_staleObjects = world.query_builder<const VidRef, const SyncedVid*>().build();

            world.system<const ActiveLevel>()
                .kind(flecs::PostLoad)
                .each([this](flecs::entity, const ActiveLevel&) {
                    reconcileStaleObjects();
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

    private:
        void reconcileStaleObjects();

        flecs::query<const VidRef, const SyncedVid*> m_staleObjects;
    };
}

// Implementation details

void reconcileDerivedState(flecs::entity entity, const VidRef& vid, int depth = 0);
void reconcileLinkedChild(flecs::entity entity, const VidRef& vid, int depth = 0);

void WorldModule::reconcileStaleObjects() {
    std::vector<flecs::entity> stale, linkedOnly;
    m_staleObjects.each([&](flecs::entity entity, const VidRef& vid, const SyncedVid* stamp) {
        if (!stamp || stamp->nvid != vid.nvid())
            stale.push_back(entity);
        else if (vid->linkedObjectVid > 0)
            linkedOnly.push_back(entity); // self-heal a missing/mismatched linked child
    });

    for (auto entity : stale)
        reconcileDerivedState(entity, entity.get<VidRef>());
    for (auto entity : linkedOnly)
        reconcileLinkedChild(entity, entity.get<VidRef>());
}

// Ensures the entity has exactly one linked child object matching vid->linkedObjectVid:
// spawns it, replaces it after a vid change, or removes a stale one. Cheap enough to run
// every frame even for already-stamped entities, which self-heals state the SyncedVid stamp
// can't know about - most notably placement: flecs clone copies components, not children.
void reconcileLinkedChild(flecs::entity entity, const VidRef& vid, int depth = 0) {
    flecs::entity linked;
    entity.children(flecs::ChildOf, [&linked](flecs::entity child) {
        if (child.has<LinkedObject>())
            linked = child;
    });

    if (!vid->linkedObjectVid) {
        if (linked)
            linked.destruct(); // vid no longer links anywhere
        return;
    }

    const auto linkedVid = vid.parent().getVid(vid->linkedObjectVid);
    if (linked) {
        const auto* linkedVidRef = linked.try_get<VidRef>();
        if (linkedVidRef && *linkedVidRef == linkedVid)
            return;

        linked.destruct(); // links to the vid's previous linkedObjectVid
    }

    auto child = entity.world()
        .entity()
        .set<Transform, Local>({
            .x = vid->linkX,
            .y = vid->linkY,
            .z = vid->linkZ,
            .direction = 0,
        })
        .emplace<VidRef>(linkedVid)
        .child_of(entity)
        .add<LinkedObject>();
    reconcileDerivedState(child, linkedVid, depth + 1);
}

// Rebuilds everything about `entity` that is derived from its VidRef: initial animation state,
// payload prototype, (Transform, World) for the transform cascade, and the linked child
// object. Idempotent and side-effect-free for callers, hence safe on live entities - this is
// what allows History to restore a vid in place instead of destroying and re-creating it.
void reconcileDerivedState(flecs::entity entity, const VidRef& vid, int depth) {
    if (depth >= 16) {
    	assert(false && "runaway linkedObjectVid chain - cyclic vid data?");
    	return;
    }

    entity.add<Transform, World>();
    entity.set<AnimationComponent>(AnimationComponent{
        .current_frame = static_cast<std::uint32_t>(std::hash<std::uint64_t>{}(entity.id()))
    });

    const auto payloadPrototype = getPayloadPrototype(vid.vid());
    if (const auto* payload = entity.try_get<GameObject::Payload>();
        !payload || payload->index() != payloadPrototype.index())
        entity.set<GameObject::Payload>(payloadPrototype);

    reconcileLinkedChild(entity, vid, depth);

    // Stamped last, so a failed rebuild leaves the entity stale (and re-reconciled later)
    // instead of silently unsynced.
    entity.set<SyncedVid>({.nvid = vid.nvid()});
}
