module;
#include <glm/glm.hpp>

export module engine.objects_view;

import std;
import utils;

import engine.bounding_box;

import Gromada.Resources;
import Gromada.VisualLogic;


// "A poor's man World" or something like that :) Will be supplemented as needed
export class ObjectsView {
public:
	struct ObjectView {
		std::reference_wrapper<const Vid> vid;
		std::reference_wrapper<const VidGraphics> graphics;
		std::reference_wrapper<const GameObject> obj;
	};

public:
	ObjectsView(std::span<const Vid> vids, const Map& map)
		: m_vids{vids}, m_map{map} {}

	void update() {
		const auto makeObjectView = [this](const GameObject& obj) -> ObjectView {
			return {
				.vid = m_vids[obj.nvid],
				.graphics = getVidGraphics(m_vids, obj.nvid),
				.obj = obj,
			};
		};

		m_objects.assign_range(m_map.objects | std::views::transform(makeObjectView));
	}

	std::ranges::bidirectional_range auto queryObjectsInRegion(BoundingBox bounds) const {
		constexpr auto visualBounds = [](const ObjectView& view) -> BoundingBox {
			const auto halfSize = glm::ivec2{view.graphics.get().imgWidth / 2, view.graphics.get().imgHeight / 2};
			return BoundingBox{
				.left = view.obj.get().x - halfSize.x,
				.right = view.obj.get().x + halfSize.x,
				.top = view.obj.get().y - halfSize.y,
				.down = view.obj.get().y + halfSize.y,
			};
		};

		return m_objects | std::views::filter([visualBounds, bounds](const ObjectView& object) { return visualBounds(object).isIntersects(bounds); });
	}

private:
	std::span<const Vid> m_vids;
	const Map& m_map;

	std::vector<ObjectView> m_objects;
};