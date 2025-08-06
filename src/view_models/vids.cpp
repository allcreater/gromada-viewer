module;
#include <imgui.h>
#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_glue.h>
#include <util/sokol_imgui.h>

export module application.view_model:vids_window;

import std;
import sokol.helpers;
import imgui_utils;
import framebuffer;
import application.model;

import Gromada.SoftwareRenderer;
import framebuffer;

import utils;

auto makeComparator(const ImGuiTableSortSpecs& sortSpecs) {
	constexpr static auto extractGraphicsGormat = [](const Vid& vid) {
		const auto* graphics = std::get_if<Vid::Graphics>(&(vid.graphicsData));
		return graphics ? graphics->get()->dataFormat : -1;
	};

	const std::array comparators{
		+[](const Vid& a, const Vid& b) { return &a <=> &b; }, // we are sure they are in the same array, so that it's safe
		+[](const Vid& a, const Vid& b) { return a.name <=> b.name; },
		+[](const Vid& a, const Vid& b) { return a.behave <=> b.behave; },
		+[](const Vid& a, const Vid& b) { return extractGraphicsGormat(a) <=> extractGraphicsGormat(b); },
	};

	return [sortSpecs, comparators](const Vid& a, const Vid& b) -> bool {
		for (auto spec : std::span{sortSpecs.Specs, static_cast<size_t>(sortSpecs.SpecsCount)}) {
			if (auto c = std::invoke(comparators[spec.ColumnIndex], a, b); c != 0)
				return spec.SortDirection == ImGuiSortDirection_Ascending ? c == std::strong_ordering::less : c == std::strong_ordering::greater;
		}
		return false;
	};
}

export class VidsWindowViewModel {
public:
	explicit VidsWindowViewModel(Model& model)
		: m_model{model} {}

	void updateUI() {
		// By default, if we don't enable ScrollX the sizing policy for each column is "Stretch"
		// All columns maintain a sizing weight, and they will occupy all available width.
		static ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
									   ImGuiTableFlags_ContextMenuInBody;

		const auto prevSelectedSection = m_selectedSection;

	    ImGui::Checkbox("Show details", &m_showDetails);
		if (ImGui::BeginTable(
				"vids_list_table", 4, ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter)) {
			ImGui::TableSetupColumn("NVID", ImGuiTableColumnFlags_WidthFixed, 30.0f);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthFixed, 30.0f);
			ImGui::TableSetupColumn("Graphics format", ImGuiTableColumnFlags_WidthFixed, 30.0f);

			ImGui::TableHeadersRow();

			if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty) {
				const Vid* selectedVid = &(m_selectedSection->get());
				std::ranges::sort(m_sortedVids, makeComparator(*specs));
				m_selectedSection = std::ranges::find(m_sortedVids, selectedVid, [](const Vid& vid) { return &vid; });
				specs->SpecsDirty = false;
			}

			for (auto it = m_sortedVids.begin(); it != m_sortedVids.end(); ++it) {
				const Vid& vid = *it;

				ImGui::TableNextColumn();
				bool isElementSelected = m_selectedSection == it;

				const auto vids = m_model.get<const GameResources>()->vids();
				const unsigned int nvid = std::distance(vids.data(), &vid);
				if (ImGui::Selectable(std::to_string(nvid).c_str(), isElementSelected, ImGuiSelectableFlags_SpanAllColumns)) {
					m_selectedSection = it;
				    //
				}
				if (isElementSelected) {
					ImGui::SetItemDefaultFocus();
				    if (std::exchange(m_selecionInvalidated, false)) {
				        ImGui::SetScrollHereY(0.5f); // Scroll to the selected item
				    }
				}

				{
					ImGuiDragDropFlags src_flags = 0;
					src_flags |= ImGuiDragDropFlags_SourceNoDisableHover;	  // Keep the source displayed as hovered
					src_flags |= ImGuiDragDropFlags_SourceNoHoldToOpenOthers; // Because our dragging is local, we disable the feature of opening foreign
																			  // treenodes/tabs while dragging
					// src_flags |= ImGuiDragDropFlags_SourceNoPreviewTooltip; // Hide the tooltip
					if (ImGui::BeginDragDropSource(src_flags)) {
					    assert(nvid < std::numeric_limits<std::uint16_t>::max());
						static std::uint8_t s_CurrentDirection = 0;
						MyImUtils::SetDragDropPayload(ObjectToPlaceMessage{
							.nvid = static_cast<std::uint16_t>(nvid),
							.direction = s_CurrentDirection,
						});

						if (std::abs(ImGui::GetIO().MouseWheel) > 0.0f) {
							const auto step = 255 / static_cast<float>(vid.directionsCount);
							s_CurrentDirection += (ImGui::GetIO().MouseWheel > 0 ? 1 : -1) * step;
						}

						ImGui::Text("dir = %i", s_CurrentDirection);
						ImGui::EndDragDropSource();
					}
				}

				ImGui::TableNextColumn();
				ImGui::Text("%s", vid.getName().c_str());

				ImGui::TableNextColumn();
				ImGui::Text("%i", vid.behave);

				ImGui::TableNextColumn();
			    ImGui::Text("%i", vid.graphics().dataFormat);

				ImGui::TableNextRow();
			}

			ImGui::EndTable();

		    if (m_showDetails) {
		        ImGui::SetNextWindowPos({320, 20}, ImGuiCond_FirstUseEver);
		        if (ImGui::Begin("Vid details", &m_showDetails)) {
					VidUI(*m_selectedSection);
					if (prevSelectedSection != m_selectedSection) {
						InvalidateSelection();
					}

					ImGui::SetNextWindowPos({10, 530}, ImGuiCond_FirstUseEver);
		            ImGui::SetNextWindowSize({300, 280}, ImGuiCond_FirstUseEver);
		            ShowFramesWindow(*m_selectedSection);
		        }
		        ImGui::End();
		    }
		}
	}

private:
	void VidUI(const Vid& self);
    void ShowFramesWindow(const Vid& self);
    void InvalidateSelection() {
        m_decodedFrames.clear();
        m_selecionInvalidated = true;
    }
	
	static std::vector<SgUniqueImage> DecodeVidFrames(const VidGraphics& vid, sg_sampler sampler);

private:
	Model& m_model;
    bool m_showDetails = false;
	std::vector<std::reference_wrapper<const Vid>> m_sortedVids{std::from_range, m_model.get<const GameResources>()->vids()};
	decltype(m_sortedVids)::iterator m_selectedSection = m_sortedVids.begin();
    bool m_selecionInvalidated = true;

	SgUniqueSampler m_guiImagesSampler{sg_sampler_desc{
		.min_filter = SG_FILTER_LINEAR,
		.mag_filter = SG_FILTER_LINEAR,
		.wrap_u = SG_WRAP_REPEAT,
		.wrap_v = SG_WRAP_REPEAT,
		.wrap_w = SG_WRAP_REPEAT,
	}};

	std::vector<SgUniqueImage> m_decodedFrames;
};

namespace {
	constexpr auto classifyUnitType(UnitType unitType) -> const char* {
		using enum UnitType;
		switch (unitType) {
			case Terrain: return "Terrain";
			case Object: return "Object";
			case Monster: return "Monster";
			case Avia: return "Avia";
			case Cannon: return "Cannon";
			case Sprite: return "Sprite";
			case Item: return "Item";
			default:
				return "???";
		}
	}
}


void VidsWindowViewModel::VidUI(const Vid& self) {
    auto linkToNvidControl = [&, vids = m_model.get<const GameResources>()->vids(), id = 0](int nvid) mutable {
        if (nvid) {
            std::array<char, 32> buffer {0};
            std::format_to_n(buffer.data(), buffer.size(), "{}", nvid);
            ImGui::PushID(id++);
            if (int index = std::abs(nvid); ImGui::TextLink(buffer.data()) && index < vids.size() ) {
                m_selectedSection = std::ranges::find(m_sortedVids, &vids[index], [](const Vid& vid) { return &vid; });
                InvalidateSelection();
            }
            ImGui::PopID();
        }
        else {
            ImGui::Text("n/a");
        }
    };


    ImGui::Text("%s", self.getName().c_str());
    ImGui::Text("unitType: %s ", classifyUnitType(self.unitType));
    ImGui::Text("Behave: %i ", self.behave);
    ImGui::Text("Flags %x", self.flags);
    ImGui::Text("Collision mask: %x", self.collisionMask);
    ImGui::Text("Sizes (W,H,Z): %i %i %i", self.sizeX, self.sizeY, self.sizeZ);
    ImGui::Text("max HP: %i", self.maxHP);
    ImGui::Text("grid radius: %i", self.gridRadius);

    ImGui::Text("Speed %i %i", self.speedX, self.speedY);
    ImGui::Text("Acceleration: %i", self.acceleration);
    ImGui::Text("Rotation period: %i", self.rotationPeriod);

    ImGui::Text("Army: %i", self.army);
    ImGui::Text("Weapon?: %i", self.someWeaponIndex);
    ImGui::Text("???: %i", self.unused2);
    ImGui::Text("Damage radius: % i", self.deathDamageRadius);
    ImGui::Text("Damage: %i", self.deathDamage);
    ImGui::Text("Linked object offset (X,Y,Z): %i %i %i", self.linkX, self.linkY, self.linkZ);

    ImGui::Text("Linked nVid: "); ImGui::SameLine(); linkToNvidControl(self.linkedObjectVid);
    ImGui::Text("Directions count: % i", self.directionsCount);
    ImGui::Text("Z Layer : % i", self.z_layer);


    ImGui::Spacing();
    constexpr std::array<const char*, 16> actionNames = {"Stand", "Build", "Go", "Start move", "L Rotate", "R Rotate", "Open", "Close", "Fight", "Salut",
        "Stand open", "Load", "Unload", "Wound", "Birth", "Death"};

    ImGui::BeginTable("Actions", 4);
    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Animation", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Child nVid", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableHeadersRow();

    for (std::size_t i = 0; i < 16; ++i) {
        ImGui::TableNextColumn();
        ImGui::Text(actionNames[i]);

        ImGui::TableNextColumn();
        ImGui::Text("%i", self.supportedActions[i]);

        ImGui::TableNextColumn();
        linkToNvidControl(self.children[i]);

        ImGui::TableNextColumn();
        ImGui::Text("%i", self.childrenCount[i]);

        ImGui::TableNextRow();
    }

    ImGui::EndTable();

    ImGui::Spacing();
    std::visit(overloaded{
                   [](std::int32_t arg) { ImGui::Text("Source nVid: %i", arg); },
                   [&self](const Vid::Graphics& arg) {
                       ImGui::Text("frames size: %i", self.dataSizeOrNvid);
                       ImGui::Text("data format: %x", arg->dataFormat);
                       ImGui::Text("frame duration: %ims (%i FPS)", arg->frameDuration, 1000 / arg->frameDuration);
                       ImGui::Text("numOfFrames: %i", arg->numOfFrames);
                       ImGui::Text("dataSize: %i", arg->dataSize);
                       ImGui::Text("width: %i", arg->width);
                       ImGui::Text("height: %i", arg->height);
                   },
               },
        self.graphicsData);
}

void VidsWindowViewModel::ShowFramesWindow(const Vid& self) {
	if (m_decodedFrames.empty()) {
	    m_decodedFrames = DecodeVidFrames(m_selectedSection->get().graphics(), m_guiImagesSampler);
	    if (m_decodedFrames.empty())
	        return;
	}

	const auto framesData = std::get_if<Vid::Graphics>(&self.graphicsData);
	if (ImGui::Begin("Decompressed images", nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) {
		std::size_t imagesPerLine = std::max(1.0f, std::floor(ImGui::GetWindowWidth() / ((*framesData)->width + 2.0f)));
		for (int index = 0; const auto& image : m_decodedFrames) {
			ImGui::Image(simgui_imtextureid(image), {static_cast<float>((*framesData)->width), static_cast<float>((*framesData)->height)});

			if ((index+1) % imagesPerLine != 0)
				ImGui::SameLine();

			index++;
		}
	}
	ImGui::End();

}

void FillWithCheckerboard(FramebufferRef framebuffer, RGBA8 color1, RGBA8 color2) {
    const size_t tile_size = 4;
    for (size_t y = 0; y < framebuffer.extent(0); ++y) {
        for (int x = 0; x < framebuffer.extent(1); ++x) {
            framebuffer[y, x] = ((x / tile_size + y / tile_size) % 2 == 0) ? color1 : color2;
        }
    }
}

std::vector<SgUniqueImage> VidsWindowViewModel::DecodeVidFrames(const VidGraphics& vid, sg_sampler sampler) {
    return vid.frames | std::views::transform([](const VidGraphics::Frame& frame) {
        Framebuffer vidFramebuffer{static_cast<int>(frame.width()), static_cast<int>(frame.height())};
        if (frame.parent->dataFormat == 3 || frame.parent->dataFormat == 4) {
            FillWithCheckerboard(vidFramebuffer, {0x5a,0x70, 0x96, 0xFF}, {0x9d, 0x4e, 0x5e, 0xFF});
        }

        DrawSprite(frame, 0, 0, vidFramebuffer);
        vidFramebuffer.commitToGpu();
        return std::move(vidFramebuffer).getImage();
    }) | std::ranges::to<std::vector>();

}