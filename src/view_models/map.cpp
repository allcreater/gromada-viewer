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

template <typename T = glm::ivec2>
constexpr T from_imvec(const ImVec2 vec) { return T{static_cast<typename T::value_type>(vec.x), static_cast<typename T::value_type>(vec.y)}; }

constexpr static ImU32 objectSelectionColor(ObjectCategory unitType);

struct SelectionRect {
    glm::ivec2 min;
    glm::ivec2 max;
};

struct IdleGesture {};
struct PanningGesture {};
struct BoxSelectGesture { SelectionRect rect; };
struct DraggingObjectsGesture { glm::ivec2 appliedWs; };
struct PlacingGesture {};
struct PaintingGesture {};
struct CancelledGesture {}; // Esc pressed mid-gesture; waits for the mouse button release
using EditorGesture = std::variant<IdleGesture, PanningGesture, BoxSelectGesture, DraggingObjectsGesture, PlacingGesture, PaintingGesture, CancelledGesture>;

// Snapshot of raw ImGui input, captured once per frame. This is the single seam through which
// map-editing logic reads user input; nothing below should call ImGui::IsKey*/IsMouse* directly.
struct FrameInput {
    glm::ivec2 mouseScreenPos;
    glm::ivec2 leftClickScreenPos;
    glm::ivec2 mouseDelta;
    glm::vec2 leftDragDelta;
    glm::fvec2 wsadDirection;
    float mouseWheel = 0.0f;
    bool leftMouseDown = false;
    bool leftMouseReleased = false;
    bool leftMouseDragging = false;
    bool isPanning = false;
    bool ctrlDown = false;
    bool shiftDown = false;
    bool deletePressed = false;
    bool escapePressed = false;
    bool windowHovered = false;
    bool mousePosValid = false;
    bool dragDropActive = false;
};

static FrameInput captureFrameInput(bool panInProgress) {
    const ImGuiIO& io = ImGui::GetIO();
    const bool ctrlDown = io.KeyCtrl;
    const bool windowHovered = ImGui::IsWindowHovered();
    const bool keyboardFree = !io.WantTextInput;
    const bool panButtonDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle);

    glm::fvec2 wsadDirection{};
    if (keyboardFree && !ctrlDown) {
        wsadDirection = { ImGui::IsKeyDown( ImGuiKey_D) - ImGui::IsKeyDown(ImGuiKey_A), ImGui::IsKeyDown( ImGuiKey_S) - ImGui::IsKeyDown(ImGuiKey_W)};
    }
    if (auto length = glm::dot(wsadDirection, wsadDirection); length > 0.0f) {
        wsadDirection *= glm::inversesqrt(length);
    }

    return FrameInput{
        .mouseScreenPos = from_imvec(ImGui::GetMousePos()),
        .leftClickScreenPos = from_imvec(io.MouseClickedPos[ImGuiMouseButton_Left]),
        .mouseDelta = from_imvec(io.MouseDelta),
        .leftDragDelta = from_imvec<glm::vec2>(ImGui::GetMouseDragDelta(ImGuiMouseButton_Left)),
        .wsadDirection = wsadDirection,
        .mouseWheel = io.MouseWheel,
        .leftMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left),
        .leftMouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left),
        .leftMouseDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left),
        .isPanning = panButtonDragging && (panInProgress || windowHovered),
        .ctrlDown = ctrlDown,
        .shiftDown = io.KeyShift,
        .deletePressed = keyboardFree && ImGui::IsKeyPressed(ImGuiKey_Delete),
        .escapePressed = keyboardFree && ImGui::IsKeyPressed(ImGuiKey_Escape),
        .windowHovered = windowHovered,
        .mousePosValid = ImGui::IsMousePosValid(),
        .dragDropActive = ImGui::IsDragDropActive(),
    };
}

export class MapViewModel {
    public:
    explicit MapViewModel(flecs::world& world) : m_world(world) {

        world.import<LevelRenderer>();

        world.system<Framebuffer, const Viewport>()
            .kind(flecs::PreUpdate)
            .each([](Framebuffer& framebuffer, const Viewport& viewport) {
                framebuffer.resize(viewport.viewportSize);
                framebuffer.clear({0, 0, 0, 0});
            });


        // TODO: is this coordinates are even used by original game?
        world.observer<const MapHeaderRawData, Camera>()
            .event(flecs::OnSet)
            .each([](flecs::entity, const MapHeaderRawData& mapHeader, Camera& viewport) {
                viewport.position =  {mapHeader.observerX, mapHeader.observerY};
            });

        m_selectionQuery = world.query_builder<const VidRef, const Transform>("selectionQuery")
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
    void updatePrototype(bool enabled, const FrameInput& input, glm::ivec2 mouseWorldPos) {
        auto prototype = m_world.target<ObjectPrototype>();
		if (!prototype.get<VidRef>())
			return;

		auto& prototype_transform = prototype.ensure<Transform, Local>();
		if (enabled) {
			prototype.enable();

		    prototype_transform.x = mouseWorldPos.x;
		    prototype_transform.y = mouseWorldPos.y;

			if (std::abs(input.mouseWheel) > 0.0f && !input.ctrlDown) {
				const auto step = 255 / static_cast<float>(prototype.get<const VidRef>()->directionsCount);
				prototype_transform.direction -= (input.mouseWheel > 0 ? 1 : -1) * step; // Reverse direction is more intuitive
			} else if (m_editorState->randomizeObjectDirection && Flags{prototype.get<const VidRef>()->flags}[ObjectFlags::RandomDirection] ) {
                prototype_transform.direction = static_cast<std::uint8_t>(randomIndex(256));
            }
		}
		else {
			prototype.disable();
		}
    }

    void onMenu() {
		if (ImGui::BeginMenu("Selection")) {
			constexpr static std::array<ObjectCategory, 7> flags = {
				ObjectCategory::Terrain, ObjectCategory::Object, ObjectCategory::Vehicle, ObjectCategory::Avia, ObjectCategory::Cannon, ObjectCategory::Sprite, ObjectCategory::Item};
			for (ObjectCategory flag : flags) {
				if (ImGui::MenuItem(to_string(flag).c_str(), nullptr, m_selectionType[flag])) {
					m_selectionType = m_selectionType ^ flag;
				}
			}

			ImGui::EndMenu();
		}

        ImGui::MenuItem("Randomize directions", nullptr, &(m_editorState->randomizeObjectDirection));
        ImGui::MenuItem("Expand tiles on drawing", nullptr, &(m_editorState->drawingAddsNewTiles));
	}

	void updateUI() {
        const auto* levelInfo = m_world.component<ActiveLevel>().try_get<MapHeaderRawData>();
        auto& viewport = m_world.get_mut<Viewport>();
        auto& camera = m_world.get_mut<Camera>();
        const FrameInput input = captureFrameInput(std::holds_alternative<PanningGesture>(m_gesture));

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        {
            Framebuffer& framebuffer = m_world.get_mut<Framebuffer>();
            framebuffer.commitToGpu();

            draw_list->AddImage(
                simgui_imtextureid(framebuffer.getImage()), ImVec2{0, 0},
                ImGui::GetMainViewport()->Size,
                ImVec2{0, 0},
                ImVec2{1, 1}, IM_COL32(255, 255, 255, 255));

            displaySelection(draw_list, viewport);
            updateSelectedObjectsPropertiesWindow(draw_list, viewport);

            displayMapBounds(draw_list, viewport, camera, levelInfo ? *levelInfo : MapHeaderRawData{});

            if (const auto* boxSelect = std::get_if<BoxSelectGesture>(&m_gesture)) {
                draw_list->AddRect(to_imvec(viewport.worldToScreenPos(boxSelect->rect.min)), to_imvec(viewport.worldToScreenPos(boxSelect->rect.max)), IM_COL32(0, 255, 0, 200));
            }
        }

        updateCamera(camera, levelInfo ? *levelInfo : MapHeaderRawData{}, input);
        updateViewport(viewport, camera);

        const auto mouseWorldPos = viewport.screenToWorldPos(input.mouseScreenPos);

        const auto clickWorldPos = viewport.screenToWorldPos(input.leftClickScreenPos);
        const EditorGesture srcGesture = std::exchange(m_gesture , updateGesture(input, mouseWorldPos, clickWorldPos, *m_editorState.get()));
        updatePrototype(std::holds_alternative<PlacingGesture>(m_gesture), input, mouseWorldPos);
        dispatchGesture( viewport, input, mouseWorldPos, srcGesture );

        if (input.deletePressed) {
            deleteSelectedObjects();
        }
    }

    EditorGesture updateGesture(const FrameInput& input, glm::ivec2 mouseWorldPos, glm::ivec2 clickWorldPos, const GlobalEditorState& editorState) const {
        // Camera panning always pre-empts whatever canvas gesture was in progress
        if (input.isPanning)
            return PanningGesture{};

        const bool isCancellable = std::holds_alternative<BoxSelectGesture>(m_gesture)
            || std::holds_alternative<DraggingObjectsGesture>(m_gesture)
            || std::holds_alternative<PaintingGesture>(m_gesture);
        if (input.escapePressed && isCancellable)
            return CancelledGesture{};

        const auto is_placementMode = std::holds_alternative<PlacementState>(editorState.state);
        const auto is_selectionMode = std::holds_alternative<SelectionState>(editorState.state);
        const auto is_terrainDrawMode = std::holds_alternative<TerrainDrawState>(editorState.state);

        const bool canStartGesture = input.windowHovered && !input.dragDropActive;

        auto enterDragGesture = [&]() -> EditorGesture {
            if (input.shiftDown || is_selectionMode)
                return BoxSelectGesture{{clickWorldPos, mouseWorldPos}};
            return DraggingObjectsGesture{};
        };

        return std::visit(overloaded{
            [&](PanningGesture gesture) -> EditorGesture {
                assert(!input.isPanning);
                return IdleGesture{}; // panning just ended
            },
            [&](IdleGesture gesture) -> EditorGesture {
                if (!canStartGesture)
                    return gesture;
                if (input.leftMouseDragging)
                    return enterDragGesture();
                if (is_placementMode && input.mousePosValid)
                   return PlacingGesture{};
                if (is_terrainDrawMode && input.leftMouseDown)
                    return PaintingGesture{};
                return gesture;
            },
            [&](BoxSelectGesture gesture) -> EditorGesture {
                if (!input.leftMouseDragging) {
                    return IdleGesture{};
                }
                gesture.rect.max = mouseWorldPos;
                return gesture;
            },
            [&](DraggingObjectsGesture gesture) -> EditorGesture {
                if (!input.leftMouseDragging) {
                    return IdleGesture{};
                }
                return gesture;
            },
            [&](PlacingGesture gesture) -> EditorGesture {
                if (!canStartGesture || !is_placementMode) {
                    return  IdleGesture{};
                }
                if (input.leftMouseDragging) {
                    return enterDragGesture();
                }
                return gesture;
            },
            [&](PaintingGesture gesture) -> EditorGesture {
                if (!input.leftMouseDown || !is_terrainDrawMode) {
                    return IdleGesture{};
                }

                return gesture;
            },
            [&](CancelledGesture gesture) -> EditorGesture {
                return input.leftMouseDown ? EditorGesture{gesture} : IdleGesture{};
            },
        }, m_gesture);
    }

    void dispatchGesture( Viewport &viewport, const FrameInput input, const glm::ivec2 mouseWorldPos, const EditorGesture srcGesture ) {
        // TODO: rewrite shorter, three visits is too much :)

        std::visit( [&]<typename DstType, typename SrcType>(const DstType& dstGesture, const SrcType& srcGesture) {
            if constexpr (AnyOf<DstType, PaintingGesture, DraggingObjectsGesture> && !std::same_as<SrcType, DstType>) {
                m_world.get_mut<History>().beginTransaction();
            }
        }, m_gesture, srcGesture);

        std::visit(overloaded{
                           [&](BoxSelectGesture gesture) {
                               applyBoxSelection(gesture.rect);
                           }, [&](DraggingObjectsGesture& gesture) {
                               const glm::ivec2 totalDragWs = glm::trunc(glm::vec2{viewport.screenToWorldMat * glm::vec3{input.leftDragDelta, 0.0f}});
                               moveSelectedObjects(totalDragWs - std::exchange(gesture.appliedWs, totalDragWs));
                           }, [&](PlacingGesture gesture) {
                               const auto& prototype = m_world.target<ObjectPrototype>();
                                if ( !input.leftMouseReleased || !prototype.get<VidRef>())
                                    return;

                               auto& history = m_world.get_mut<History>();
                               std::unique_lock transaction{history};
                               history.stage(prototype.clone().child_of(m_world.component<ActiveLevel>()), true);
                           }, [&](PaintingGesture gesture) {
                               const auto vid = m_editorState->selectedNvid;
                               if (!vid || (vid->type == ObjectClass::Terrain && Flags{vid->flags}[ObjectFlags::RandomDirection]))
                                   insertTile(m_world, vid, mouseWorldPos.x, mouseWorldPos.y, m_editorState->drawingAddsNewTiles);
                           }, [&]<typename DstType>(const DstType& dstGesture) {
                           }
                   }, m_gesture);


        std::visit( [&]<typename DstType, typename SrcType>(const DstType& dstGesture, const SrcType& srcGesture) {
            if constexpr (AnyOf<SrcType, PaintingGesture, DraggingObjectsGesture> && !std::same_as<SrcType, DstType>) {
                auto& history = m_world.get_mut<History>();
                if constexpr (std::same_as<DstType, CancelledGesture>)
                    history.rollbackTransaction();
                else
                    history.commitTransaction();
            }
        }, m_gesture, srcGesture);

        const bool gestureJustCancelled = std::holds_alternative<CancelledGesture>(m_gesture) && !std::holds_alternative<CancelledGesture>(srcGesture);
        if (input.escapePressed && !gestureJustCancelled) {
            m_world.remove_all<Selected>();
            m_editorState->state = SelectionState{};
            m_world.modified<GlobalEditorState>();
        }
    }

    auto computeBBScreenSize (const Viewport& viewport, const Vid& vid, const Transform& worldTransform, auto&& boundsGetter) {
        BoundingBox bb = std::invoke(boundsGetter, vid, worldTransform);
        return std::make_tuple(to_imvec(viewport.worldToScreenPos({bb.left, bb.top})), to_imvec(viewport.worldToScreenPos({bb.right, bb.down})));
    };

    void displaySelection(ImDrawList* draw_list, const Viewport& viewport) {
        m_selectionUIState.selectedObjects.clear();

        m_selectionQuery.each([&](flecs::entity id, const Vid& vid, const Transform& worldTransform) {
            const auto  color = objectSelectionColor(vid.category);
            const float rounding = std::min(vid.sizeX, vid.sizeY) * 0.25f;

            auto [min, max] = computeBBScreenSize(viewport, vid, worldTransform, PhysicalBoundsFn{});
            draw_list->AddRectFilled(min, max, color, rounding);

            if (id.has<GameObject::Payload>()) {
                //draw_list->AddLine(to_imvec(viewport.worldToScreenPos(pos)), ImGui::GetWindow(), IM_COL32(255, 255, 255, 255), 2.0f);

                m_selectionUIState.selectedObjects.push_back( id );
            }
        });
    }

    void updateSelectedObjectsPropertiesWindow(ImDrawList* draw_list, Viewport& viewport) {
        if (m_selectionUIState.selectedObjects.empty()) {
            m_selectionUIState.selectedObject = -1;
        } else {
            m_selectionUIState.selectedObject = std::clamp(m_selectionUIState.selectedObject, 0, static_cast<int>(m_selectionUIState.selectedObjects.size()) - 1);

            ImGui::SetNextWindowPos({500, 200}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2{200, 300}, ImGuiCond_FirstUseEver);
            ImGui::Begin("Selected object");
            MyImUtils::ComboBox( "object" , &m_selectionUIState.selectedObject, std::span{m_selectionUIState.selectedObjects}, [&](flecs::entity obj) {
                return obj.get<VidRef>()->getName();
            } );

            auto objectHandle = m_selectionUIState.selectedObjects[m_selectionUIState.selectedObject];

            auto& vidComponent = objectHandle.get<VidRef>();
            auto& transform = objectHandle.get<Transform, World>();

            if (ImGui::Button("Center camera")) {
                m_world.get_mut<Camera>().position = {transform.x, transform.y};
            }

            ImGui::SameLine( );
            if (ImGui::Button(std::format("Select nvid [{}]", vidComponent.nvid()).c_str())) {
                m_editorState->selectedNvid = vidComponent;
                m_world.modified<GlobalEditorState>();
                flushDerivedState(m_world);
            }

            ImGui::SameLine( );
            if (ImGui::Button("Delete")) {
                auto& history = m_world.get_mut<History>();
                std::unique_lock transaction{history};
                history.stage(objectHandle);
                setMapObjectEnabled(objectHandle, false); // soft delete - undoable

                // TODO: this early exit is actually a crutch :(
                ImGui::End();
                return;
            }

            auto [min, max] = computeBBScreenSize(viewport, vidComponent, transform, VisualBoundsFn{});
            draw_list->AddRect(min, max, IM_COL32(100, 255, 100, 255), 0.0f, ImDrawFlags_None, 2.0f);

            {
                auto& history = m_world.get_mut<History>();
                std::unique_lock transaction{history};
                history.stage(objectHandle); // before any of the mutations below, per History's contract

                processObjectTransformTab( objectHandle.get_mut<Transform, Local>() ); // NOTE: yes, the local transform of the root object is world transform
                processObjectPropertiesTab(objectHandle);
            }

            ImGui::End();
        }
    }

    void processObjectTransformTab(Transform& worldTransform) {
        if (!ImGui::CollapsingHeader("Transform"))
            return;

        static_assert(sizeof(worldTransform.x) == sizeof(int32_t));
        ImGui::InputScalar("X",  ImGuiDataType_S32, &worldTransform.x);
        ImGui::InputScalar("Y",  ImGuiDataType_S32, &worldTransform.y);
        ImGui::InputScalar("Z",  ImGuiDataType_S32, &worldTransform.z);
        constexpr static std::uint8_t minDirection = 0, maxDirection = 255;
        ImGui::SliderScalar( "Direction", ImGuiDataType_U8, &worldTransform.direction, &minDirection, &maxDirection );
    }

    void processObjectPropertiesTab(flecs::entity objectHandle ) {
        std::visit( overloaded{
        []( Payloads::VisualObject ) {
            ImGui::Text("Just a visual object, no payload");
        },
        []( Payloads::MaterialObject &payload ) {
            ImGui::InputScalar("Actual HP",  ImGuiDataType_U8, &payload.hp);
        },
        [this, objectHandle]( Payloads::AssetObject &payload ) {
            if (ImGui::BeginTabBar("PayloadTabs")) {
                if (ImGui::BeginTabItem("General")) {
                    ImGui::PushItemWidth(100.0f);
                    ImGui::InputScalar("Actual HP",  ImGuiDataType_U8, &payload.hp);
                    ImGui::InputScalar("Build time",  ImGuiDataType_U8, &payload.buildTime);
                    ImGui::InputScalar("Army",  ImGuiDataType_U8, &payload.army );
                    ImGui::InputScalar("Behavior",  ImGuiDataType_U8, &payload.behave );
                    ImGui::PopItemWidth();

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Commands")) {
                    MyImUtils::ListBox("commands", &m_selectionUIState.currentCommand, std::span{payload.commands}, MyImUtils::MakeSelectableCallback<const ObjectCommand>([&](const ObjectCommand& cmd) {
                        return std::format("[{:3}] {:^10}\t{}\t{}", std::distance(const_cast<const ObjectCommand*>(payload.commands.data()), &cmd), to_string(cmd.command), cmd.p1, cmd.p2);
                    }));

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Items")) {
                    MyImUtils::ListBox("items", &m_selectionUIState.currentItem, std::span{payload.items},MyImUtils::MakeSelectableCallback<std::int16_t>( [&gr = m_world.get<const GameResources>()](std::int16_t nvid) {
                        return std::format("[{:3}] {}", nvid, gr.getVid(nvid)->getName());
                    } ), {-FLT_MIN, ImGui::GetContentRegionAvail().y - 50.0f});

                    if (ImGui::Button("+")) {
                        payload.items.push_back( m_editorState->selectedNvid.nvid());
                    }
                    ImGui::SameLine();

                    ImGui::BeginDisabled(payload.items.empty());
                    if (ImGui::Button("-")) {
                        payload.items.erase(payload.items.begin() + m_selectionUIState.currentItem);
                    }
                    ImGui::EndDisabled();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();

                bool isEnabled = objectHandle.has<SquadMember>();
                ImGui::Checkbox( "Squad", &isEnabled);
                objectHandle.add_if<SquadMember>(isEnabled);
                if (isEnabled) {
                    auto& squad = objectHandle.get_mut<SquadMember>();
                    ImGui::SameLine();
                    ImGui::InputScalar("##SquadNumber", ImGuiDataType_U16, &squad.number);
                }
            }
        }
        }, objectHandle.get_mut<GameObject::Payload>() );
    }

    void displayMapBounds(ImDrawList* draw_list, const Viewport& viewport, const Camera& camera, const MapHeaderRawData& mapHeader) {
        const auto margins = glm::ivec2{50, 50} * camera.magnificationFactor;
        const auto vp_min = viewport.worldToScreenPos(glm::ivec2{});
        const auto vp_max = viewport.worldToScreenPos(glm::ivec2{mapHeader.width, mapHeader.height });

		ImGui::RenderRectFilledWithHole(
			draw_list, ImRect{to_imvec(vp_min - margins), to_imvec(vp_max + margins)}, ImRect{to_imvec(vp_min), to_imvec(vp_max)}, IM_COL32(255, 0, 0, 100), 0);
	}

    static void updateCamera(Camera& camera, const MapHeaderRawData& mapHeader, const FrameInput& input) {
        if (!input.dragDropActive && input.ctrlDown && input.mouseWheel != 0.0f) {
            const auto newMagnification = std::clamp(camera.magnificationFactor + static_cast<int>(glm::sign(input.mouseWheel)), 1, 8);
            const auto screenCenter = glm::vec2{from_imvec(ImGui::GetMainViewport()->Size)} * 0.5f;
            const auto cursorOffset = glm::vec2{input.mouseScreenPos} - screenCenter;

            camera.position += cursorOffset * (1.0f / camera.magnificationFactor - 1.0f / newMagnification);
            camera.magnificationFactor = newMagnification;
        }

        const auto magnification = static_cast<float>(camera.magnificationFactor);
        if (input.isPanning) {
            camera.position -= glm::vec2{input.mouseDelta} / magnification;
        }

        camera.velocity += (input.wsadDirection * 8000.0f / magnification - camera.velocity*5.0f) * ImGui::GetIO().DeltaTime;
        camera.position += camera.velocity * ImGui::GetIO().DeltaTime;
        //camera.velocity = camera.velocity * 0.93f;

        camera.position = glm::clamp(camera.position, glm::vec2{0, 0}, glm::vec2{mapHeader.width, mapHeader.height});

    }

    // NOTE: implicedly uses ImGui::GetMainViewport() to get the viewport size
    static void updateViewport(Viewport& vp, const Camera& camera) {
        const auto magnificationFactor = camera.magnificationFactor;
        vp.viewportSize = from_imvec(ImGui::GetMainViewport()->Size) / magnificationFactor;

        vp.viewportPos = glm::ivec2{camera.position} - vp.viewportSize / 2;
        vp.screenToWorldMat = glm::mat3x3{
            1.0f / magnificationFactor, 0.0f, 0.0f,
            0.0f, 1.0f / magnificationFactor, 0.0f,
            vp.viewportPos.x, vp.viewportPos.y, 1.0f,
        };
        vp.worldToScreenMat = glm::inverse(vp.screenToWorldMat);
    }

    void applyBoxSelection(const SelectionRect& rect) {
        m_world.remove_all<Selected>();
        m_world.defer([&] {
            m_world.get<ObjectsView>().queryObjectsInRegion(ObjectsView::physicalBounds, BoundingBox::fromPositions(rect.min.x, rect.min.y, rect.max.x, rect.max.y), [this](flecs::entity entity) {
                if (entity.has(flecs::ChildOf, m_world.component<ActiveLevel>()) && m_selectionType[entity.get<const VidRef>()->category]) {
                    entity.add<Selected>();
                }
            });
        });
    }

    void moveSelectedObjects(glm::ivec2 delta_ws) {
        if (delta_ws == glm::ivec2{})
            return;

        m_selectionQuery.each([delta_ws, &history = m_world.get_mut<History>()](flecs::entity id, const Vid& vid, const Transform& _) {
            history.stage(id);
            auto& transform_ls = id.get_mut<Transform, Local>();

            transform_ls.x += delta_ws.x;
            transform_ls.y += delta_ws.y;
        });
    }

    void deleteSelectedObjects() {
        auto& history = m_world.get_mut<History>();
        std::unique_lock transaction{history};

        m_world.defer([&] {
            m_selectionQuery.each([&history](flecs::entity id, const Vid& vid, const Transform& _) {
                history.stage(id);
                setMapObjectEnabled(id, false); // soft delete - undoable
            });
        });
    }


    flecs::world& m_world;
    flecs::ref<GlobalEditorState> m_editorState = m_world.get_ref<GlobalEditorState>();
    flecs::query<const VidRef, const Transform> m_selectionQuery;
    EditorGesture m_gesture = IdleGesture{};
    Flags<ObjectCategory> m_selectionType {0b01111110}; // Default selection type

    struct SelectionUIState {
        int currentCommand = 0;
        int currentItem = 0;
        int selectedObject = 0;
        std::vector<flecs::entity> selectedObjects;
    } m_selectionUIState;
};

constexpr static ImU32 objectSelectionColor(ObjectCategory unitType) {
    using enum ObjectCategory;
    constexpr auto alpha = 70;
    switch (unitType) {
    case Terrain: return IM_COL32(50, 200, 50, alpha);
    case Object: return IM_COL32(200, 200, 200, alpha);
    case Vehicle: return IM_COL32(255, 100, 100, alpha);
    case Avia: return IM_COL32(50, 100, 200, alpha);
    case Cannon: return IM_COL32(128, 80, 50, alpha);
    case Sprite: return IM_COL32(100, 100, 100, alpha);
    case Item: return IM_COL32(200, 200, 50, alpha);
    default:
        return IM_COL32_BLACK;
    }
}