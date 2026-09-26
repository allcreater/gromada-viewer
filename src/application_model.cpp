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
export struct SquadMember {
	std::uint16_t number = 0;
};

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
// A snapshot of everything that makes an entity "a map object"
struct ObjectSnapshot {
    GameObject object;
    std::optional<std::uint32_t> orderingIndex;
    std::optional<std::uint16_t> squadNumber;

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
	void stage(flecs::entity entity, bool isNew = false) {
		assert(!m_savepoints.empty() && "stage() called outside a transaction");

		const auto currentTransactionStart = m_savepoints.back();
		const bool alreadyStagedHere = std::any_of(m_current.begin() + currentTransactionStart, m_current.end(),
			[&](const ChangeRecord& change) { return change.entity == entity; });
		if (alreadyStagedHere)
			return;

		if (isNew)
			m_current.push_back({.entity = entity, .before = std::nullopt, .after = snapshot(entity)});
		else
			m_current.push_back({.entity = entity, .before = snapshot(entity), .after = std::nullopt});
	}

	void commitTransaction() {
		assert(!m_savepoints.empty() && "commitTransaction() without a matching beginTransaction()");
		m_savepoints.pop_back();
		if (!m_savepoints.empty())
			return; // an enclosing transaction is still open - it will diff/push on its own commit

		std::erase_if(m_current, [this](ChangeRecord& change) {
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
	// affecting an enclosing transaction's earlier records.
	void rollbackTransaction() {
		assert(!m_savepoints.empty() && "rollbackTransaction() without a matching beginTransaction()");
		const auto savepoint = m_savepoints.back();
		m_savepoints.pop_back();

		for (auto i = m_current.size(); i-- > savepoint; )
			apply(m_current[i].entity, m_current[i].before);
		m_current.erase(m_current.begin() + savepoint, m_current.end());
	}

	bool canUndo() const { return !m_undoStack.empty(); }
	bool canRedo() const { return !m_redoStack.empty(); }

	void undo() {
		if (m_undoStack.empty())
			return;

		auto step = std::move(m_undoStack.back());
		m_undoStack.pop_back();
		for (auto& change : step | std::views::reverse)
			apply(change.entity, change.before);
		m_redoStack.push_back(std::move(step));
	}

	void redo() {
		if (m_redoStack.empty())
			return;

		auto step = std::move(m_redoStack.back());
		m_redoStack.pop_back();
		for (auto& change : step)
			apply(change.entity, change.after);
		m_undoStack.push_back(std::move(step));
	}

	// Drops all history. Meant for whole-document replacement (new/load map), where old entity
	// handles are invalidated anyway and there is nothing meaningful left to undo into.
	void clear() {
		assert(m_savepoints.empty() && "clear() called while a transaction is open");
		m_undoStack.clear();
		m_redoStack.clear();
	}

private:
	// Absent snapshot == the object is not part of the document: never had a vid, or is
	// soft-deleted (disabled).
	static std::optional<ObjectSnapshot> snapshot(flecs::entity entity) {
		if (!entity.is_alive() || !entity.enabled())
			return std::nullopt;

		const auto* vid = entity.try_get<VidRef>();
		const auto* transform = entity.try_get<Transform, Local>();
		const auto* payload = entity.try_get<GameObject::Payload>();
		if (!vid ||	!transform || !payload)
			return std::nullopt;

		const auto* ordering = entity.try_get<EditorOrdering>();
		const auto* squadMember = entity.try_get<SquadMember>();
		return ObjectSnapshot{
			.object = makeGameObject(*vid, *transform, payload, ordering ? ordering->uid : 0),
			.orderingIndex = ordering ? std::optional{ordering->index} : std::nullopt,
			.squadNumber = squadMember ? std::optional{squadMember->number} : std::nullopt,
		};
	}

	// Applies `state` to `entity` in place. Deletion is soft (disable instead of destruct), so
	// an object's handle stays valid for as long as the history exists, and no redirect
	// bookkeeping is needed anywhere. A vid change goes through instantiateObject(), the same
	// GameObject -> components step used for loading maps: WorldModule's reconciliation system
	// rebuilds the derived state (animation, payload prototype, linked child) on the next
	// flushDerivedState(), and the restored payload survives that reconciliation because the
	// reconciler only replaces a payload whose variant type doesn't fit the vid's class.
	static void apply(flecs::entity entity, const std::optional<ObjectSnapshot>& state) {
		if (!entity.is_alive()) {
			assert(false && "map object destroyed outside history - deletions must go through setMapObjectEnabled()");
			return;
		}

		setMapObjectEnabled(entity, state.has_value());
		if (!state) {
			return;
		}

		instantiateObject(entity.world(), state->object, entity);

		if (state->orderingIndex)
			entity.set<EditorOrdering>({.uid = state->object.id, .index = *state->orderingIndex});
		else
			entity.remove<EditorOrdering>();

		if (state->squadNumber)
			entity.set<SquadMember>({.number = *state->squadNumber});
		else
			entity.remove<SquadMember>();
	}

	friend class std::unique_lock<History>;
	void lock() { beginTransaction(); }
	void unlock() { commitTransaction(); }

	std::vector<std::size_t> m_savepoints;
	std::vector<ChangeRecord> m_current;
	std::unordered_set<std::uint64_t> m_touched;

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
		world.component<SquadMember>();
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

		assert((map.armies[0].squads == map.armies[1].squads) && "Not an error, but original game maps have same squads arrays");
		// NOTE: we deliberately build map by armies[1] - it's because original game also reads only last available squads list.
		const auto objectIdToSquadIndex = [&squads = map.armies[1].squads] {
			if (squads.size() > std::numeric_limits<std::uint16_t>::max())
				throw std::length_error{"Too many Squad objects in map"};

			std::unordered_map<std::uint32_t, std::uint16_t> result;
			for (std::uint32_t squadIndex = 0; squadIndex < squads.size(); ++squadIndex) {
				for (const auto& objectId : squads[squadIndex]) {
					const auto [it, isInserted] = result.emplace(objectId, static_cast<std::uint16_t>(squadIndex));
					assert(isInserted);
				}
			}
			return result;
		}();

	    for (const auto& obj : map.objects) {
	    	auto entity = instantiateObject(*this, obj)
	    		.set<EditorOrdering>({.uid = obj.id, .index = static_cast<std::uint16_t>(&obj - map.objects.data())});

	    	if (auto it = objectIdToSquadIndex.find(obj.id); it != objectIdToSquadIndex.end()) {
	    		entity.set<SquadMember>(SquadMember{.number = it->second});
	    	}
	    }

	    activeLevel.set<MapHeaderRawData>(map.header);
        activeLevel.set<Path>(std::move(path));
	    activeLevel.set<Armies>(std::move(map.armies));

	    flushDerivedState(*this);
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

	    // First step - collect all known objects and try to place them in the correct order.
	    query.each([&](flecs::entity entity, const VidRef& vid, const Transform& transform) {
	        if (auto* existing_object_attribs = entity.try_get<EditorOrdering>()) {
	            const auto [id, index] = *existing_object_attribs;
	            if (index < objects.size() && objects[index].id() == 0) { // in case of collision - jyst treat object as unordered
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
	        if (unordered_objects.empty())
	            throw std::logic_error("Model::prepareObjectsToExport: ran out of objects to fill a free slot - internal ordering inconsistency");

	        objects[i] = unordered_objects.front();
	        {
	            auto& ordering = objects[i].ensure<EditorOrdering>();
	            ordering.index = static_cast<std::uint32_t>(i);
	            if (ordering.uid == 0)
	                ordering.uid = generate_new_id();
	        }
	        unordered_objects.pop_front();
	    }
	    if (!unordered_objects.empty())
	        throw std::logic_error("Model::prepareObjectsToExport: leftover objects after filling all free slots - internal ordering inconsistency");

	    return objects;
	}

    Map saveMap() {
	    const auto activeLevel = this->component<ActiveLevel>();
	    auto& header = activeLevel.ensure<MapHeaderRawData>();

        auto entities = prepareObjectsToExport();

	    updateMapBounds(entities, header); // translate coordinates so that (0,0) is top-left corner of the map
	    updateArmies(entities, activeLevel.ensure<Armies>());

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
            .armies = activeLevel.get<Armies>(),
        };
	}

private:
    static void updateMapBounds(std::ranges::range auto&& entities, MapHeaderRawData& header) {
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

	static void updateArmies(std::ranges::range auto&& entities, Armies& armies) {
    	assert(armies[0].squads == armies[1].squads);

    	auto squadsMap = entities
    		| std::views::filter([](flecs::entity obj) { return obj.has<SquadMember>(); })
    		| std::views::transform([](flecs::entity obj) {return std::pair{obj.get<SquadMember>().number, obj.get<EditorOrdering>().uid}; })
    		| std::ranges::to<std::multimap>();

    	armies[0].squads = armies[1].squads = squadsMap
    		| std::views::chunk_by([](const auto& a, const auto& b) { return a.first == b.first; })
    		| std::views::transform([](auto&& squad) {
    			return squad | std::views::values | std::ranges::to<Army::Squad>();
    		})
    		| std::ranges::to<std::vector>();
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

    	world.add<ObjectPrototype>(world.entity().emplace<VidRef>());

    	world.observer<GlobalEditorState>()
			.event(flecs::OnSet)
			.each([world](flecs::entity _, const GlobalEditorState& state) {
				world.target<ObjectPrototype>().set<VidRef>(state.selectedNvid);
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

	world.defer([&] {
		world.get<ObjectsView>().queryObjectsInRegion(ObjectsView::physicalBounds, region, [&](flecs::entity entity) {
			if (!ok)
				return;

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
					entity.set<VidRef>(tile)
						.set<Transform, Local>({.x = transform->x, .y = transform->y, .z = 0, .direction = tile.direction});
				} else {
					// The substitution erases the tile entirely - a soft delete, so that undo
					// can restore it.
					setMapObjectEnabled(entity, false);
				}
			});
		});
	});

	if (ok)
		history.commitTransaction();
	else
		history.rollbackTransaction();

    flushDerivedState(world);
}