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

import utils;

auto makeComparator(const ImGuiTableSortSpecs& sortSpecs) {
	constexpr static auto extractGraphicsGormat = [](const Vid& vid) {
		const auto* graphics = std::get_if<Vid::Graphics>(&(vid.graphicsData));
		return graphics ? graphics->get()->visualBehavior : -1;
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

		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
			if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && m_selectedSection != m_sortedVids.begin())
				m_selectedSection--;
			if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && m_selectedSection + 1 != m_sortedVids.end())
				m_selectedSection++;
		}

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
				m_selectedSection = std::ranges::find(m_sortedVids, selectedVid, [](auto reference) { return &(reference.get()); });
				specs->SpecsDirty = false;
			}

			for (auto it = m_sortedVids.begin(); it != m_sortedVids.end(); ++it) {
				const Vid& vid = *it;

				ImGui::TableNextColumn();
				bool isElementSelected = m_selectedSection == it;
				const unsigned int nvid = std::distance(m_model.vids().data(), &vid);
				if (ImGui::Selectable(std::to_string(nvid).c_str(), isElementSelected, ImGuiSelectableFlags_SpanAllColumns)) {
					m_selectedSection = it;
				}
				if (isElementSelected) {
					ImGui::SetItemDefaultFocus();
				}

				{
					ImGuiDragDropFlags src_flags = 0;
					src_flags |= ImGuiDragDropFlags_SourceNoDisableHover;	  // Keep the source displayed as hovered
					src_flags |= ImGuiDragDropFlags_SourceNoHoldToOpenOthers; // Because our dragging is local, we disable the feature of opening foreign
																			  // treenodes/tabs while dragging
					// src_flags |= ImGuiDragDropFlags_SourceNoPreviewTooltip; // Hide the tooltip
					if (ImGui::BeginDragDropSource(src_flags)) {
						static std::uint8_t s_CurrentDirection = 0;
						MyImUtils::SetDragDropPayload(ObjectToPlaceMessage{
							.nvid = nvid,
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
				ImGui::Text("%s", vid.name.data());

				ImGui::TableNextColumn();
				ImGui::Text("%i", vid.behave);

				ImGui::TableNextColumn();

				std::visit(overloaded{
							   [](std::int32_t arg) { ImGui::Text("Source nVid: %i", arg); },
							   [](const Vid::Graphics& arg) { ImGui::Text("%i", arg->visualBehavior); },
						   },
					vid.graphicsData);

				ImGui::TableNextRow();
			}

			ImGui::EndTable();

			if (ImGui::Begin("Vid details")) {
				if (prevSelectedSection != m_selectedSection) {
					m_decodedFrames.clear(); // To reduce sokol's pool size

					if (const auto pFramesData = std::get_if<Vid::Graphics>(&(m_selectedSection->get().graphicsData)); pFramesData && *pFramesData) {
						m_decodedFrames = DecodeVidFrames(**pFramesData, m_guiImagesSampler);
					}
				}

				VidUI(*m_selectedSection);
			}
			ImGui::End();
		}
	}

private:
	void VidUI(const Vid& self);
	
	static std::vector<SgUniqueImage> DecodeVidFrames(const VidGraphics& vid, sg_sampler sampler);

private:
	Model& m_model;

	std::vector<std::reference_wrapper<const Vid>> m_sortedVids{m_model.vids().begin(), m_model.vids().end()};
	decltype(m_sortedVids)::iterator m_selectedSection = m_sortedVids.begin();

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
	constexpr auto classifyVisualBehavior (std::uint8_t behavior) -> const char* {
		switch (behavior & 0x3F) {
		case 0:
			return "unpack mode 1";
		case 6:
			return "unpack mode 2";
		case 8:
			return "unpack mode 3";
		default:
			return "not packed";
		}
	};

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
	ImGui::Text("%s", self.name.data());
	ImGui::Text("unitType: %s ", classifyUnitType(self.unitType));
	ImGui::Text("Behave: %i ", self.behave);
	ImGui::Text("Flags % i", self.flags);
	ImGui::Text("Collision mask: %x", self.collisionMask);
	ImGui::Text("Sizes (W,H,Z): %i %i %i", self.anotherWidth, self.anotherHeight, self.z_or_height);
	ImGui::Text("max HP: %i", self.maxHP);
	ImGui::Text("grid radius: %i", self.gridRadius);
	ImGui::Text("???: %i", self.p6);
	ImGui::Text("Speed % i", self.speed);

	ImGui::Text("???: %i", self.hz1);
	ImGui::Text("???: %i", self.hz2);
	ImGui::Text("???: %i", self.hz3);
	ImGui::Text("Army: %i", self.army);
	ImGui::Text("Weapon?: %i", self.someWeaponIndex);
	ImGui::Text("???: %i", self.hz4);
	ImGui::Text("Death size margin: % i", self.deathSizeMargin);
	ImGui::Text("Death ??: %i", self.somethingAboutDeath);
	ImGui::Text("some X: %i", self.sX);
	ImGui::Text("some Y: %i", self.sY);
	ImGui::Text("some Z: %i", self.sZ);

	ImGui::Text("???: %i", self.hz5);
	ImGui::Text("???: %i", self.hz6);
	ImGui::Text("Directions count: % i", self.directionsCount);
	ImGui::Text("z2 : % i", self.z);


	ImGui::Spacing();
	ImGui::Text("Actions: ");
    constexpr std::array<const char*, 16> actionNames = {"Stand", "Build", "Go", "Start move", "L Rotate", "R Rotate", "Open", "Close", "Fight", "Salut",
		"Stand open", "Load", "Unload", "Wound", "Birth", "Death"};
	for (std::size_t i = 0; i < 16; ++i) {
		ImGui::Text("    %s: %i", actionNames[i], self.supportedActions[i]);
	}

	std::visit(overloaded{
				   [](std::int32_t arg) { ImGui::Text("Source nVid: %i", arg); },
				   [&self](const Vid::Graphics& arg) {
					   ImGui::Text("frames size: %i", self.dataSizeOrNvid);
					   ImGui::Text("Visual behavior: %x (%s)", arg->visualBehavior, classifyVisualBehavior(arg->visualBehavior));
					   ImGui::Text("???: %i", arg->hz7);
					   ImGui::Text("numOfFrames: %i", arg->numOfFrames);
					   ImGui::Text("dataSize: %i", arg->dataSize);
					   ImGui::Text("imgWidth: %i", arg->imgWidth);
					   ImGui::Text("imgHeight: %i", arg->imgHeight);
				   },
			   },
		self.graphicsData);

	if (m_decodedFrames.empty())
		return;

	const auto framesData = std::get_if<Vid::Graphics>(&self.graphicsData);
	static ImVec2 lastWindowSize = {100, 100};
	ImGui::SetNextWindowSize(lastWindowSize, ImGuiCond_Appearing);
	if (ImGui::Begin("Decompressed images", nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) {
		std::size_t imagesPerLine = std::max(1.0f, std::floor(ImGui::GetWindowWidth() / ((*framesData)->imgWidth + 2.0f)));
		for (int index = 0; const auto& image : m_decodedFrames) {
			ImGui::Image(simgui_imtextureid(image), {static_cast<float>((*framesData)->imgWidth), static_cast<float>((*framesData)->imgHeight)});

			if ((index+1) % imagesPerLine != 0)
				ImGui::SameLine();

			index++;
		}

		lastWindowSize = ImGui::GetWindowSize();
	}
	ImGui::End();

}

std::vector<SgUniqueImage> VidsWindowViewModel::DecodeVidFrames(const VidGraphics& vid, sg_sampler sampler) {
	return vid.decode() | std::views::transform([&](const std::vector<RGBA8>& data) -> SgUniqueImage {
		return sg_image_desc{.type = SG_IMAGETYPE_2D,
			.width = static_cast<int>(vid.imgWidth),
			.height = static_cast<int>(vid.imgHeight),
			.num_slices = 1,
			.pixel_format = SG_PIXELFORMAT_RGBA8,
			.data = {{{{.ptr = data.data(), .size = data.size() * sizeof(RGBA8)}}}}};
	}) | std::ranges::to<std::vector>();

}