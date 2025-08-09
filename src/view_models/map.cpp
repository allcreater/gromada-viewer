module;
#include <flecs.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_glue.h>
#include <util/sokol_imgui.h>

export module application.view_model : map;

import std;
import framebuffer;
import imgui_utils;
import utils;

import application.model;
import engine.bounding_box;
import engine.level_renderer;
import engine.objects_view;

import Gromada.Actions;
import Gromada.Map;
import Gromada.SoftwareRenderer;

constexpr ImVec2 to_imvec(const auto vec) { return ImVec2{static_cast<float>(vec.x), static_cast<float>(vec.y)}; }
constexpr glm::ivec2 from_imvec(const ImVec2 vec) { return glm::ivec2{static_cast<int>(vec.x), static_cast<int>(vec.y)}; }

constexpr static ImU32 objectSelectionColor(UnitType unitType);

struct SelectionRect {
    glm::ivec2 min;
    glm::ivec2 max;
};

export class MapViewModel {
    public:
    explicit MapViewModel(flecs::world& world) : m_world(world) {

        world.import<LevelRenderer>();

        world.system<Framebuffer, const Viewport>()
            .term_at(0).singleton()
            .term_at(1).singleton()
            .kind(flecs::PreUpdate)
            .each([](Framebuffer& framebuffer, const Viewport& viewport) {
                framebuffer.resize(viewport.viewportSize);
                framebuffer.clear({0, 0, 0, 0});
            });


        // TODO: is this coordinates are even used by original game?
        world.observer<const MapHeaderRawData, Viewport>()
            .term_at(1).singleton().filter()
            .event(flecs::OnSet)
            .each([](flecs::entity, const MapHeaderRawData& mapHeader, Viewport& viewport) {
                viewport.camPos =  {mapHeader.observerX, mapHeader.observerY};
            });

        m_selectionQuery = world.query_builder<const VidComponent, const Transform>()
            .with<Selected>()
            .term_at(1).second<World>()
            .cached().build();

        // world.system<Framebuffer>()
        //     .kind(0)//(flecs::OnStore)
        //     .each([](Framebuffer& framebuffer) {
        //         framebuffer.commitToGpu();
        //         ImGui::Image(simgui_imtextureid(framebuffer.getImage()), ImVec2{0, 0});
        //     });
    }

    void handleDragDrop(const Viewport& viewport) {
        const auto mouseWorldPos = viewport.screenToWorldPos(from_imvec(ImGui::GetMousePos()));

        const auto* vp = ImGui::GetMainViewport();
        if (ImGui::BeginDragDropTargetCustom(ImRect{vp->WorkPos, vp->WorkSize}, vp->ID)) {
            ImGuiDragDropFlags target_flags = 0;
            target_flags |= ImGuiDragDropFlags_AcceptBeforeDelivery;
            //target_flags |= ImGuiDragDropFlags_AcceptNoDrawDefaultRect; // Don't display the yellow rectangle
            if (const auto payload = MyImUtils::AcceptDragDropPayload<ObjectToPlaceMessage>(target_flags)) {
                auto _ = m_world.entity()
                .set<Transform, Local>({
                    .x = static_cast<std::int16_t>(mouseWorldPos.x),
                    .y = static_cast<std::int16_t>(mouseWorldPos.y),
                    .z = 0,
                    .direction = payload->first.direction,
                })
                .emplace<VidComponent>(*m_world.get<const GameResources>(), payload->first.nvid)
                .child_of<ActiveLevel>()
                .add_if<DestroyAfterUpdate>(!(payload->second->IsDelivery()));

            }
            ImGui::EndDragDropTarget();
        }
    }

    void onMenu() {
        if (ImGui::BeginMenu("Selection")) {
			constexpr static std::array<UnitType, 7> flags = {
				UnitType::Terrain, UnitType::Object, UnitType::Monster, UnitType::Avia, UnitType::Cannon, UnitType::Sprite, UnitType::Item};
			for (UnitType unitType : flags) {
				const auto flag = std::to_underlying(unitType);
				bool isSelected = (m_selectionType & flag) != 0;
				if (ImGui::MenuItem(to_string(unitType).data(), nullptr, isSelected)) {
					m_selectionType ^= flag;
				}
			}

            ImGui::EndMenu();
        }
    }

    void updateUI() {
        auto levelInfo = m_world.component<ActiveLevel>().get<MapHeaderRawData>();
        auto viewport = m_world.get_mut<Viewport>();

        if (!ImGui::IsDragDropActive() && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::GetIO().MouseWheel != 0.0f) {
            viewport->magnificationFactor += static_cast<int>(glm::sign(ImGui::GetIO().MouseWheel));
        }

        handleDragDrop(*viewport);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        {
            Framebuffer& framebuffer = *(m_world.get_mut<Framebuffer>());
            framebuffer.commitToGpu();

            draw_list->AddImage(
                simgui_imtextureid(framebuffer.getImage()), ImVec2{0, 0},
                ImGui::GetMainViewport()->Size,
                ImVec2{0, 0},
                ImVec2{1, 1}, IM_COL32(255, 255, 255, 255));

            displaySelection(draw_list, *viewport);
        }

        updateViewport(*viewport, levelInfo ? *levelInfo : MapHeaderRawData{});
        if (!ImGui::IsDragDropActive() ) {
            if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                updateSelection(viewport->screenToWorldPos(from_imvec(ImGui::GetMousePos())));
            } else {
                moveSelectedObjects(*viewport);
            }
        }

    }

    void displaySelection(ImDrawList* draw_list, const Viewport& viewport) {
        m_selectionQuery.each([&](flecs::entity id, const Vid& vid, const Transform& transform) {
            const glm::ivec2 halfSize {vid.sizeX / 2, vid.sizeY / 2};
            const glm::ivec2 pos {transform.x, transform.y};

            const auto color = objectSelectionColor(vid.unitType);
            const float rounding = std::min(halfSize.x, halfSize.y) * 0.5f;
            draw_list->AddRectFilled(to_imvec(viewport.worldToScreenPos(pos - halfSize)), to_imvec(viewport.worldToScreenPos(pos + halfSize)), color, rounding);

            if (const auto* payload = id.get<GameObject::Payload>(); payload && !payload->commands.empty()) {
                ImGui::SetNextWindowPos(to_imvec(viewport.worldToScreenPos(pos)));
                ImGui::PushID(payload);
                ImGui::BeginChild("Commands", ImVec2{200.0f, payload->commands.size() * 25.0f}, ImGuiChildFlags_FrameStyle);
                std::ranges::for_each(payload->commands, [&, index = 0](const ObjectCommand& cmd) mutable {
                    ImGui::TextUnformatted(std::format("[{:3}] {:^10}\t{}\t{}", index++, to_string(cmd.command), cmd.p1, cmd.p2).c_str());
                });
                ImGui::EndChild();
                ImGui::PopID();
            }
        });

        if (m_selectionFrame) {
            draw_list->AddRect(to_imvec(viewport.worldToScreenPos(m_selectionFrame->min)), to_imvec(viewport.worldToScreenPos(m_selectionFrame->max)), IM_COL32(0, 255, 0, 200));
        }
    }

    // NOTE: implicedly uses ImGui::GetMainViewport() to get the viewport size
    static void updateViewport(Viewport& vp, const MapHeaderRawData& mapHeader) {
        const auto magnificationFactor = vp.magnificationFactor = std::clamp(vp.magnificationFactor, 1, 8);
        vp.viewportSize = from_imvec(ImGui::GetMainViewport()->Size) / magnificationFactor;

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
            vp.camPos -= from_imvec(ImGui::GetIO().MouseDelta);
        }

        vp.camPos = glm::clamp(vp.camPos, glm::ivec2{0, 0}, glm::ivec2{mapHeader.width, mapHeader.height});


        vp.viewportPos = vp.camPos - vp.viewportSize / 2;
        vp.screenToWorldMat = glm::mat3x3{
            1.0f / magnificationFactor, 0.0f, 0.0f,
            0.0f, 1.0f / magnificationFactor, 0.0f,
            vp.viewportPos.x, vp.viewportPos.y, 1.0f,
        };
        vp.worldToScreenMat = glm::inverse(vp.screenToWorldMat);
    }

    void updateSelection(glm::ivec2 mouseWorldPos) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            if (!m_selectionFrame) {
                m_selectionFrame = {mouseWorldPos, mouseWorldPos};
            }
            else {
                m_selectionFrame->max = mouseWorldPos;
                m_world.remove_all<Selected>();
                m_world.defer([&] {
                    m_world.get<ObjectsView>()->queryObjectsInRegion(ObjectsView::physicalBounds, BoundingBox::fromPositions(m_selectionFrame->min.x, m_selectionFrame->min.y, m_selectionFrame->max.x, m_selectionFrame->max.y), [this](flecs::entity entity) {
                        if (entity.has(flecs::ChildOf, m_world.component<ActiveLevel>()) && (std::to_underlying((*entity.get<const VidComponent>())->unitType) & m_selectionType) != 0) {
                            entity.add<Selected>();
                        }
                    });
                });
            }
        }
        else if (m_selectionFrame) {
            m_selectionFrame.reset();
        }
    }

    void moveSelectedObjects(const Viewport& viewport) {
        const auto delta_ws = viewport.screenToWorldMat * glm::vec3{from_imvec(ImGui::GetMouseDragDelta(0)), 0.0f};
        m_selectionQuery.each([delta_ws](flecs::entity id, const Vid& vid, const Transform& _) {
            auto transform_ls = id.get_mut<Transform, Local>();
            assert(transform_ls);

            transform_ls->x += static_cast<int>(delta_ws.x);
            transform_ls->y += static_cast<int>(delta_ws.y);
        });
        ImGui::ResetMouseDragDelta();
    }

    flecs::world& m_world;
    flecs::query<const VidComponent, const Transform> m_selectionQuery;
    std::optional<SelectionRect> m_selectionFrame;
    std::underlying_type_t<UnitType> m_selectionType = 0b01111110; // Default selection type
};

constexpr static ImU32 objectSelectionColor(UnitType unitType) {
    using enum UnitType;
    constexpr auto alpha = 70;
    switch (unitType) {
    case Terrain: return IM_COL32(50, 200, 50, alpha);
    case Object: return IM_COL32(200, 200, 200, alpha);
    case Monster: return IM_COL32(255, 100, 100, alpha);
    case Avia: return IM_COL32(50, 100, 200, alpha);
    case Cannon: return IM_COL32(128, 80, 50, alpha);
    case Sprite: return IM_COL32(100, 100, 100, alpha);
    case Item: return IM_COL32(200, 200, 50, alpha);
    default:
        return IM_COL32_BLACK;
    }
}