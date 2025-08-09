module;
#include <flecs.h>
#include <glm/glm.hpp>

export module engine.level_renderer;

import std;

import framebuffer;

import engine.bounding_box;
import engine.objects_view;
import engine.world_components;

import Gromada.Resources;
import Gromada.SoftwareRenderer;
import Gromada.VisualLogic;

import utils;

export struct Viewport {
    glm::ivec2 camPos{0.0f, 0.0f};
    glm::ivec2 viewportSize;
    int magnificationFactor = 1; // actual range is from 1 to 8

    // derivatives
    glm::ivec2 viewportPos;
    glm::mat3x3 screenToWorldMat, worldToScreenMat;

    [[nodiscard]] glm::ivec2 screenToWorldPos(glm::ivec2 screenPos) const noexcept {
        return screenToWorldMat * glm::vec3{screenPos, 1.0f};
    }
    [[nodiscard]] glm::ivec2 worldToScreenPos(glm::ivec2 worldPos) const noexcept {
        return worldToScreenMat * glm::vec3{worldPos, 1.0f};
    }
};

export class LevelRenderer {
public:
	LevelRenderer(const flecs::world& world) {
	    struct RenderOrder {
	        auto operator <=>(const RenderOrder&) const = default;
	        RenderOrder() = default;
	        RenderOrder(const Transform& transform, const Vid& vid) : m_tuple{vid.z_layer, transform.y + vid.graphics().height / 10 + transform.z} {}

	    private:
	        std::tuple<unsigned char, int> m_tuple;
	    };
	    world.component<RenderOrder>();
	    world.system<const Transform, const VidComponent>()
	        .term_at(0).second<World>()
            .kind(flecs::OnUpdate)
	        .write<RenderOrder>()
            .each([](flecs::entity entity, const Transform& world_transform, const Vid& vid) {
                entity.ensure<RenderOrder>() = {world_transform, vid};
        });

	    world.component<Viewport>();
	    world.set<Viewport>({.viewportSize = {1024, 768}});

	    world.component<Framebuffer>();
	    world.set<Framebuffer>({1024, 768});

	    // const auto time = std::chrono::high_resolution_clock::now();
	    // const auto renderDuration = std::chrono::high_resolution_clock::now() - time;
	    world.system<Framebuffer, const Viewport, const Transform, const VidComponent, const AnimationComponent>()
            .term_at(0).singleton()
            .term_at(1).singleton()
	        .term_at(2).second<World>()
            .kind(flecs::PreStore)
            .with<const RenderOrder>().order_by<const RenderOrder>([](flecs::entity_t, const RenderOrder* a, flecs::entity_t, const RenderOrder* b) -> int { return ordering_to_int(*a <=> *b);})
            .each([](Framebuffer& framebuffer, const Viewport& viewport, const Transform& transform, const Vid& vid, const AnimationComponent& animation) {
                const glm::ivec2 pos = glm::ivec2{transform.x - vid.graphics().width / 2, transform.y - vid.graphics().height / 2 - transform.z} - viewport.viewportPos;
                DrawSprite(vid.graphics().frames[animation.current_frame], pos.x, pos.y, framebuffer);
        });
	}
};