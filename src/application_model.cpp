module;
#include <flecs.h>

export module application.model;

import std;
import utils;

import Gromada.ResourceReader;
import Gromada.Map;
import Gromada.Terromorphing;
import engine.bounding_box;
import engine.level_renderer;
import engine.audio;

export import Gromada.GameResources;

export import engine.world_components;

export struct EditorOrdering {
    std::uint32_t uid = 0;
    std::uint32_t index = 0;

    bool operator==(const EditorOrdering&) const = default;
};
export struct Selected {};
export struct ObjectPrototype {};
export using Path = std::filesystem::path;
export using Armies = std::array<Army, 2>;

export struct SelectionState {};
export struct PlacementState {};
export struct TerrainDrawState {};

export struct GlobalEditorState {
	VidRef selectedNvid;
	std::variant<SelectionState, PlacementState, TerrainDrawState> state;
	bool randomizeObjectDirection = true;
};

export void flushDerivedState(flecs::world& world) {
	world.progress(0.0f);
}


// TODO: move to separate module after MSVC update - was strange internal compuler errors with flecs headers
//
// A snapshot of everything that makes an entity "a map object", as opposed to the lossy on-disk
// GameObject DTO (int16_t coordinates, id-only payload reference etc). Entities with no VidRef
// (i.e. not a map object at all) are represented by an absent ObjectSnapshot, not a default one.
struct ObjectSnapshot {
    VidRef vid;
    Transform transform;
    EditorOrdering ordering;
    GameObject::Payload payload;

    bool operator==(const ObjectSnapshot&) const = default;
};

struct ChangeRecord {
    flecs::entity entity;
    std::optional<ObjectSnapshot> before;
    std::optional<ObjectSnapshot> after;
};

using UndoStep = std::vector<ChangeRecord>;

// Generic undo/redo for map-object edits (create/modify/delete under ActiveLevel), built on
// explicit staging rather than flecs OnSet observers: by the time an OnSet observer runs, the
// new value is already written, so there is no generic way to recover "before" from one - call
// sites must call stage() themselves, immediately before they touch an existing entity (or right
// after creating a new, still-empty one).
//
// beginTransaction()/commitTransaction() nest like savepoints: only the outermost commit actually
// diffs and pushes a step, so a multi-frame gesture (map.cpp) can wrap many inner calls (e.g. one
// insertTile per frame) into a single undo step, while each inner call remains independently
// correct - and rollbackTransaction() undoes only what was staged since ITS OWN beginTransaction(),
// leaving an enclosing transaction's earlier progress untouched. This also replaces bespoke
// all-or-nothing bookkeeping in callers: on failure, call rollbackTransaction() instead of
// tracking a "did everything succeed" flag by hand.
export class History {
public:
	void beginTransaction() {
		m_savepoints.push_back(m_current.size());
	}

	// Captures `entity`'s current state as "before" the first time it's staged in the current
	// (innermost) transaction; later calls for the same entity within it are no-ops. Scoped to
	// just this transaction's own records (from its savepoint onward) - not to the whole nested
	// stack - so a later sibling transaction (e.g. the next frame's insertTile, nested inside one
	// long painting-gesture transaction) can still stage an entity a previous, already-committed
	// sibling transaction touched: that entity may have changed since, and this transaction needs
	// its own "before" to be able to roll itself back independently of the earlier one.
	void stage(flecs::entity entity) {
		entity = resolve(entity);
		assert(!m_savepoints.empty() && "stage() called outside a transaction");

		const auto currentTransactionStart = m_savepoints.back();
		const bool alreadyStagedHere = std::any_of(m_current.begin() + currentTransactionStart, m_current.end(),
			[&](const ChangeRecord& change) { return change.entity == entity; });
		if (alreadyStagedHere)
			return;

		m_current.push_back({.entity = entity, .before = snapshot(entity), .after = std::nullopt});
	}

	// Declares that `to` now represents whatever `from` used to represent. A vid change is always
	// destroy + recreate (see apply()), which mints a fresh entity id - so without this, a still-
	// pending record elsewhere (an older, not-yet-undone step, or this transaction's own earlier
	// stage() of `from`) that remembers `from` would go stale the moment something else destroys
	// it, and its eventual undo would silently miss the entity that actually needs touching.
	void redirect(flecs::entity from, flecs::entity to) {
		if (from != to)
			m_redirects[from.id()] = to;
	}

	void commitTransaction() {
		assert(!m_savepoints.empty() && "commitTransaction() without a matching beginTransaction()");
		m_savepoints.pop_back();
		if (!m_savepoints.empty())
			return; // an enclosing transaction is still open - it will diff/push on its own commit

		std::erase_if(m_current, [this](ChangeRecord& change) {
			change.entity = resolve(change.entity);
			change.after = snapshot(change.entity);
			return change.before == change.after;
		});

		if (!m_current.empty()) {
			m_undoStack.push_back(std::move(m_current));
			m_redoStack.clear();
		}
		m_current.clear();
	}

	// Restores whatever was staged since the matching beginTransaction() and discards it, without
	// affecting an enclosing transaction's earlier records. Restoring a "before" that still has a
	// value can destroy-and-recreate the entity (see apply()); when an enclosing transaction is
	// still open, that recreated entity is re-staged into it so it stays tracked instead of
	// silently becoming an untracked (if currently correct-looking) part of the document.
	void rollbackTransaction() {
		assert(!m_savepoints.empty() && "rollbackTransaction() without a matching beginTransaction()");
		const auto savepoint = m_savepoints.back();
		m_savepoints.pop_back();
		const bool hasEnclosingTransaction = !m_savepoints.empty();

		std::vector<flecs::entity> toAdopt;
		for (auto i = m_current.size(); i-- > savepoint; ) {
			const auto target = resolve(m_current[i].entity);
			const auto restored = apply(target, m_current[i].before);
			redirect(target, restored);
			if (hasEnclosingTransaction && m_current[i].before)
				toAdopt.push_back(restored);
		}
		m_current.erase(m_current.begin() + savepoint, m_current.end());

		for (auto entity : toAdopt)
			stage(entity);
	}

	bool canUndo() const { return !m_undoStack.empty(); }
	bool canRedo() const { return !m_redoStack.empty(); }

	void undo() {
		if (m_undoStack.empty())
			return;

		auto step = std::move(m_undoStack.back());
		m_undoStack.pop_back();
		for (auto& change : step | std::views::reverse) {
			const auto target = resolve(change.entity);
			change.entity = apply(target, change.before);
			redirect(target, change.entity);
		}
		m_redoStack.push_back(std::move(step));
	}

	void redo() {
		if (m_redoStack.empty())
			return;

		auto step = std::move(m_redoStack.back());
		m_redoStack.pop_back();
		for (auto& change : step) {
			const auto target = resolve(change.entity);
			change.entity = apply(target, change.after);
			redirect(target, change.entity);
		}
		m_undoStack.push_back(std::move(step));
	}

	// Drops all history. Meant for whole-document replacement (new/load map), where old entity
	// handles are invalidated anyway and there is nothing meaningful left to undo into.
	void clear() {
		assert(m_savepoints.empty() && "clear() called while a transaction is open");
		m_undoStack.clear();
		m_redoStack.clear();
		m_redirects.clear();
	}

private:
	// Follows the redirect chain to whatever entity currently represents `entity`, compacting
	// every visited hop to point straight at the result so later lookups stay cheap.
	flecs::entity resolve(flecs::entity entity) {
		std::vector<std::uint64_t> visited;
		for (auto it = m_redirects.find(entity.id()); it != m_redirects.end(); it = m_redirects.find(entity.id())) {
			visited.push_back(entity.id());
			entity = it->second;
		}
		for (auto id : visited)
			m_redirects[id] = entity;
		return entity;
	}

	static std::optional<ObjectSnapshot> snapshot(flecs::entity entity) {
		if (!entity.is_alive())
			return std::nullopt;

		const auto* vid = entity.try_get<VidRef>();
		if (!vid)
			return std::nullopt;

		const auto* transform = entity.try_get<Transform, Local>();
		const auto* ordering = entity.try_get<EditorOrdering>();
		const auto* payload = entity.try_get<GameObject::Payload>();
		return ObjectSnapshot{
			.vid = *vid,
			.transform = transform ? *transform : Transform{},
			.ordering = ordering ? *ordering : EditorOrdering{},
			.payload = payload ? *payload : GameObject::Payload{},
		};
	}

	// Applies `state` to `entity`, returning the (possibly new) handle that now represents it.
	// A vid change is always destroy + recreate, never an in-place VidRef set: WorldModule's
	// OnSet<VidRef> observer (world_components.cppm) resets Payload and spawns a linked child
	// object every time VidRef is set, which is only safe for a brand-new entity - exactly the
	// destroy+recreate discipline the rest of the codebase (e.g. tile substitution) already follows.
	static flecs::entity apply(flecs::entity entity, const std::optional<ObjectSnapshot>& state) {
		const auto* currentVid = entity.is_alive() ? entity.try_get<VidRef>() : nullptr;

		if (!state) {
			if (entity.is_alive())
				entity.destruct();
			return entity;
		}

		if (!currentVid || *currentVid != state->vid) {
			flecs::world world = entity.world();
			if (entity.is_alive())
				entity.destruct();
			entity = world.entity().set<VidRef>(state->vid).child_of(world.component<ActiveLevel>());
		}

		entity.set<Transform, Local>(state->transform);
		entity.set<EditorOrdering>(state->ordering);
		entity.set<GameObject::Payload>(state->payload);
		return entity;
	}

	std::vector<std::size_t> m_savepoints;
	std::vector<ChangeRecord> m_current;
	std::unordered_map<std::uint64_t, flecs::entity> m_redirects;

	std::vector<UndoStep> m_undoStack;
	std::vector<UndoStep> m_redoStack;
};

export struct EditorComponents {
    EditorComponents(flecs::world& world) {
        world.component<EditorOrdering>();
        world.component<Selected>();
		world.component<ObjectPrototype>();
		world.component<Path>();
		world.component<Armies>().set(flecs::Singleton);
    	world.component<GlobalEditorState>().set(flecs::Singleton);
		world.component<AudioEngine>().set(flecs::Singleton);
		world.component<History>().set(flecs::Singleton);
	}
};

export class Model : public flecs::world {
public:
	explicit Model(std::filesystem::path path)
		: flecs::world{create_world(std::move(path))} {}

    void newMap(VidRef vid, VidRef substrate, int width, int height) {
	    const auto activeLevel = this->component<ActiveLevel>();
	    this->delete_with(flecs::ChildOf, activeLevel);
	    this->get_mut<History>().clear();

	    if (width < 0 || height < 0)
            throw std::invalid_argument("Model::newMap: width and height must be non-negative");

	    if (vid) {
	        generateDefaultTerrain(vid, width, height);
	    }

	    const auto fullWidth = (vid ? vid->sizeX : 1) * width;
	    const auto fullHeight = (vid ? vid->sizeY : 1) * height;
	    if (fullWidth > std::numeric_limits<std::int16_t>::max() || fullHeight > std::numeric_limits<std::int16_t>::max()) {
	        throw std::invalid_argument("Model::newMap: resulting map size is too large");
	    }

		if (substrate) {
			generateDefaultTerrain(substrate, fullWidth / substrate->sizeX, fullHeight / substrate->sizeY, /*isSubstrate=*/true);
		}

	    //activeLevel.set<Path>({});
	    activeLevel.set<Armies>({});
	    activeLevel.set<MapHeaderRawData>(MapHeaderRawData{
            .width = static_cast<std::uint32_t>(fullWidth),
            .height = static_cast<std::uint32_t>(fullHeight),
            .observerX = static_cast<std::int16_t>(std::midpoint(fullWidth, 0)),
            .observerY = static_cast<std::int16_t>(std::midpoint(fullHeight, 0)),
        });

	    // debug
#if 0
		const auto vids = this->get<const GameResources>().baseTilesVids();
		for (int j = 0; j < vids.size(); ++j) {
			for (int i = 0; i < vids.size(); ++i) {
				const auto transition = getTilesTransition( vids[i], vids[j]);
				if (!transition)
					continue;

				this->entity()
				.set<Transform, Local>({.x = i * 80, .y = j * 50 - 200, .z = 0, .direction = 0})
				.emplace<VidRef>(transition->targetVid)
				.child_of(activeLevel);
			}
		}
#endif

	    flushDerivedState(*this);
	}

    // TODO: "this->" leaved to remember that it will be a free function soon
	void loadMap(std::filesystem::path path) {
		const auto& gameResources = this->get<const GameResources>();
		const auto map = ::loadMap(gameResources.vids(), path);
	    const auto activeLevel = this->component<ActiveLevel>();
	    this->delete_with(flecs::ChildOf, activeLevel);
	    this->get_mut<History>().clear();

	    for (const auto& obj : map.objects) {
	        this->entity()
                .emplace<VidRef>(gameResources.getVid(obj.nvid))
	            .set<Transform, Local>({.x = obj.x, .y = obj.y, .z = obj.z, .direction = obj.direction})
	            .set<GameObject::Payload>(obj.payload)
	            .set<EditorOrdering>({.uid = obj.id, .index = static_cast<std::uint16_t>(&obj - map.objects.data())})
                .child_of(activeLevel);
	    }

	    activeLevel.set<MapHeaderRawData>(map.header);
        activeLevel.set<Path>(std::move(path));
	    activeLevel.set<Armies>(std::move(map.armies));

	    flushDerivedState(*this);
	}

    static GameObject makeGameObject(const VidRef& vid, const Transform& transform, const GameObject::Payload* payload, std::uint32_t id) {
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
	    auto query = this->query_builder<const VidRef, const Transform>()
            .term_at(1).second<Local>() // Or world? Anyway, should be the same for top-level objects
            .with(flecs::ChildOf).second<ActiveLevel>()
            .build();

	    std::unordered_set<std::uint32_t> known_ids;
	    std::deque<flecs::entity> unordered_objects;
	    std::vector<flecs::entity> objects(query.count());

	    // First step - collect all known objects and try to place them in the correct order
	    query.each([&](flecs::entity entity, const VidRef& vid, const Transform& transform) {
	        if (auto* existing_object_attribs = entity.try_get<EditorOrdering>()) {
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

        auto generate_new_id = [&known_ids] {
            std::uniform_int_distribution<std::uint32_t> dist{1, std::numeric_limits<std::uint32_t>::max()};
            std::uint32_t id;
            do {
                id = dist(randomEngine());
            } while (!known_ids.insert(id).second);
            return id;
        };

	    // Second step - fill in the gaps with new objects
	    auto free_indices = std::views::iota(std::size_t{0}, objects.size()) | std::views::filter([&objects](std::size_t index) { return objects[index].id() == 0; });
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
	    updateMapBounds(entities, header);

	    return Map {
	        .header = header,
			.objects = entities | std::views::transform([i = 0](const flecs::entity& entity) mutable {
				const auto& vid = entity.get<VidRef>();
				const auto& transform = entity.get<Transform, Local>();
				const auto* payload = entity.try_get<GameObject::Payload>();
				const auto& ordering = entity.get<EditorOrdering>();
				assert(ordering.index == i++);
				return makeGameObject(vid, transform, payload, ordering.uid);
			}) | std::ranges::to<std::vector<GameObject>>(),
            .armies = activeLevel.ensure<Armies>(),
        };
	}

private:
    void updateMapBounds(std::ranges::range auto&& entities, MapHeaderRawData& header) {
        const auto map_bounds = std::reduce(entities.begin(), entities.end(), BoundingBox{}, [](BoundingBox bb, flecs::entity obj) {
            const auto& transform = obj.get<Transform, Local>();
            return bb.extend(transform.x, transform.y);
        });

        std::ranges::for_each(entities, [&map_bounds](flecs::entity obj) {
        	auto& transform = obj.get_mut<Transform, Local>();
            transform.x -= map_bounds.left;
            transform.y -= map_bounds.top;
        });

        header.height = map_bounds.height();
        header.width = map_bounds.width();
        header.observerX -= map_bounds.left;
        header.observerY -= map_bounds.top;
    }

    void generateDefaultTerrain(VidRef vid, int width, int height, bool isSubstrate = false) {
        const auto activeLevel = this->component<ActiveLevel>();

        assert(vid->category == ObjectCategory::Terrain);

    	//std::uint32_t baseIndex = this->count<EditorOrdering>();
        for (int j = 0; j < height; ++j) {
            for (int i = 0; i < width; ++i) {
                auto e = this->entity()
                    .set<VidRef>(vid)
                    .set<Transform, Local>({.x = static_cast<std::int16_t>(i * vid->sizeX + vid->sizeX / 2), .y = static_cast<std::int16_t>(j * vid->sizeY + vid->sizeY / 2), .z = 0, .direction = static_cast<std::uint8_t>(randomIndex(256))})
                    //.set<EditorOrdering>({.uid = 0, .index = baseIndex++})
                    .child_of(activeLevel);
            }
        }
    }

	flecs::world create_world(std::filesystem::path resourcesPath) {
		flecs::world world{};
        world.import<WorldModule>();
        world.import<EditorComponents>();

        world.emplace<GameResources>(resourcesPath);
        world.emplace<AudioEngine>();
    	world.emplace<GlobalEditorState>();
    	world.emplace<History>();

    	world.observer<GlobalEditorState>()
			.event(flecs::OnSet)
			.each([world](flecs::entity _, const GlobalEditorState& state) {
				if (world.target<ObjectPrototype>().is_valid())
					world.target<ObjectPrototype>().destruct();

				if (state.selectedNvid) {
					auto prototype = world.entity().emplace<VidRef>(state.selectedNvid);

					world.add<ObjectPrototype>(prototype);
				}
			});

        return world;
    }

};

export void insertTile(flecs::world& world, VidRef baseTerrainTile, int x, int y) {
	const auto& substrateVids = world.get<const GameResources>().substrateTilesVids();
	const auto& baseTiles = world.get<const GameResources>().baseTilesVids();
	if (std::ranges::find(baseTiles, baseTerrainTile) == baseTiles.end())
		return;

	auto& history = world.get_mut<History>();
	history.beginTransaction();
	bool ok = true;

	const auto referenceSizeTile = baseTiles[1]; //TODO: crutch
	const auto region = BoundingBox::fromPositions(x - referenceSizeTile->sizeX/2, y - referenceSizeTile->sizeY/2, x + referenceSizeTile->sizeX/2, y + referenceSizeTile->sizeY/2);

	// Structural changes (destruct, adding VidRef to a new entity) can't happen inline while a
	// query is iterating the same table - defer() queues them and applies them once iteration ends.
	world.defer([&] {
		world.get<ObjectsView>().queryObjectsInRegion(ObjectsView::physicalBounds, region, [&](flecs::entity entity) {
			if (!ok)
				return;

			// flecs::pair<Transform, Local> is needed (rather than plain Transform) because Transform is only ever stored as a (Transform, Local/World) pair
			entity.get([&](const VidRef& vid, const flecs::pair<Transform, Local>& transform) {
				if ((vid && (vid->category != ObjectCategory::Terrain || std::ranges::find(substrateVids, vid) != substrateVids.end())) || !entity.has(flecs::ChildOf, world.component<ActiveLevel>()))
					return;

				const auto getDirection = [](int deltaX, int deltaY) -> CornerDirection {
					return deltaX > 0 ? (deltaY > 0 ? CornerDirection::BottomRight : CornerDirection::TopRight) : (deltaY > 0 ? CornerDirection::BottomLeft : CornerDirection::TopLeft);
				};

				const Tile sourceTile{vid, transform->direction};
				const auto substitution = trySubstituteTile(sourceTile, baseTerrainTile, getDirection(transform->x - x, transform->y - y));
				if (!substitution) {
					ok = false;
					return;
				}
				if (*substitution == sourceTile)
					return;

				history.stage(entity);
				if (const auto tile = *substitution) {
					auto newTile = world.entity();
					history.stage(newTile); // before any component is set, so "before" means "didn't exist"
					history.redirect(entity, newTile); // an earlier, still-undoable edit may still remember `entity`
					newTile.set<VidRef>(tile)
						.set<Transform, Local>({.x = transform->x, .y = transform->y, .z = 0, .direction = tile.direction})
						.child_of(world.component<ActiveLevel>());

					if (auto ordering = entity.try_get<EditorOrdering>()) {
						newTile.set<EditorOrdering>(*ordering);
					}
				}
				entity.destruct();
			});
		});
	});

	if (ok)
		history.commitTransaction();
	else
		history.rollbackTransaction();

    flushDerivedState(world);
}