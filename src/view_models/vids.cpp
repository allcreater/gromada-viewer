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

import application.model;

import utils;


export class VidsWindowViewModel {
public:
	explicit VidsWindowViewModel(Model& model)
		: m_model{model} {}

	void updateUI() {
		// By default, if we don't enable ScrollX the sizing policy for each column is "Stretch"
		// All columns maintain a sizing weight, and they will occupy all available width.
		static ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
									   ImGuiTableFlags_ContextMenuInBody;

		ImGui::Begin("Vids");

		const auto prevSelectedSection = m_selectedSection;

		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
			if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
				m_selectedSection = std::max(m_selectedSection - 1, 0);
			if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
				m_selectedSection = std::min(m_selectedSection + 1, static_cast<int>(m_model.vids().size()));
		}

		if (ImGui::BeginTable("vids_list_table", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter)) {
			ImGui::TableSetupColumn("NVID", ImGuiTableColumnFlags_WidthFixed, 50.0f);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthFixed, 50.0f);
			ImGui::TableSetupColumn("Graphics format", ImGuiTableColumnFlags_WidthFixed, 50.0f);

			ImGui::TableHeadersRow();


			// ImGui::TableGetSortSpecs()
			// TODO: add sorting

			for (int index = 0; const auto& vid : m_model.vids()) {
				ImGui::TableNextColumn();

				bool isElementSelected = m_selectedSection == index;
				if (ImGui::Selectable(std::to_string(index).c_str(), isElementSelected, ImGuiSelectableFlags_SpanAllColumns)) {
					m_selectedSection = index;
				}
				if (isElementSelected) {
					ImGui::SetItemDefaultFocus();
				}

				ImGui::TableNextColumn();
				ImGui::Text("%s", vid.name.data());

				ImGui::TableNextColumn();
				ImGui::Text("%i", vid.behave);

				ImGui::TableNextColumn();

				std::visit(overloaded{
							   [](std::int32_t arg) { ImGui::Text("Source nVid: %i", arg); },
							   [](const VidRawData::Graphics& arg) { ImGui::Text("%i", arg->visualBehavior); },
						   },
					vid.graphicsData);

				ImGui::TableNextRow();
				index++;
			}

			ImGui::EndTable();

			ImGui::Begin("Vid details");
			if (m_selectedSection >= 0 && m_selectedSection < m_model.vids().size()) {
				const auto& vid = m_model.vids()[m_selectedSection];
				if (prevSelectedSection != m_selectedSection) {
					m_decodedFrames.reset(); // To reduce sokol's pool size

					if (const auto pFramesData = std::get_if<VidRawData::Graphics>(&vid.graphicsData); pFramesData && *pFramesData) {
						m_decodedFrames = DecodeVidFrames(**pFramesData, m_guiImagesSampler);
					}
				}

				VidUI(vid);
			}
			ImGui::End();
		}
		ImGui::End();
	}

private:
	void VidUI(const VidRawData& self);
	
	struct DecodedFrames;
	static DecodedFrames DecodeVidFrames(const VidGraphics& vid, sg_sampler sampler);

private:
	Model& m_model;

	int m_selectedSection = 0;

	SgUniqueSampler m_guiImagesSampler{sg_sampler_desc{
		.min_filter = SG_FILTER_LINEAR,
		.mag_filter = SG_FILTER_LINEAR,
		.wrap_u = SG_WRAP_REPEAT,
		.wrap_v = SG_WRAP_REPEAT,
		.wrap_w = SG_WRAP_REPEAT,
	}};
	struct DecodedFrames {
		std::vector<SgUniqueImage> images;
	};
	std::optional<DecodedFrames> m_decodedFrames;
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


void VidsWindowViewModel::VidUI(const VidRawData& self) {
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
				   [&self](const VidRawData::Graphics& arg) {
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

	if (!m_decodedFrames)
		return;

	const auto framesData = std::get_if<VidRawData::Graphics>(&self.graphicsData);
	static ImVec2 lastWindowSize = {100, 100};
	ImGui::SetNextWindowSize(lastWindowSize, ImGuiCond_Appearing);
	if (ImGui::Begin("Decompressed images", nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) {
		std::size_t imagesPerLine = std::max(1.0f, std::floor(ImGui::GetWindowWidth() / ((*framesData)->imgWidth + 2.0f)));
		for (int index = 0; const auto& image : m_decodedFrames->images) {
			ImGui::Image(simgui_imtextureid(image), {static_cast<float>((*framesData)->imgWidth), static_cast<float>((*framesData)->imgHeight)});

			if ((index+1) % imagesPerLine != 0)
				ImGui::SameLine();

			index++;
		}

		lastWindowSize = ImGui::GetWindowSize();
	}
	ImGui::End();

}

VidsWindowViewModel::DecodedFrames VidsWindowViewModel::DecodeVidFrames(const VidGraphics& vid, sg_sampler sampler) {

	auto images = vid.decode() | std::views::transform([&](const std::vector<RGBA8>& data) -> SgUniqueImage {
		return sg_image_desc{.type = SG_IMAGETYPE_2D,
			.width = static_cast<int>(vid.imgWidth),
			.height = static_cast<int>(vid.imgHeight),
			.num_slices = 1,
			.pixel_format = SG_PIXELFORMAT_RGBA8,
			.data = {{{{.ptr = data.data(), .size = data.size() * sizeof(RGBA8)}}}}};
	}) | std::ranges::to<std::vector>();

	return {
		.images = std::move(images),
	};
}