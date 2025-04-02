module;
#include <imgui.h>

export module application.view_model:vids_window;

import std;
import sokol;
import sokol.helpers;
import sokol.imgui;
import imgui_utils;

import application.model;

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

			for (auto [index, vid] : m_model.vids() | std::views::enumerate) {
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
				ImGui::Text("%i", vid.visualBehavior);


				ImGui::TableNextRow();
			}

			ImGui::EndTable();

			ImGui::Begin("Vid details");
			if (m_selectedSection >= 0 && m_selectedSection < m_model.vids().size()) {
				const auto& vid = m_model.vids()[m_selectedSection];
				if (prevSelectedSection != m_selectedSection) {
					m_decodedFrames.reset(); // To reduce sokol's pool size
					m_decodedFrames = DecodeVidFrames(vid, m_guiImagesSampler);
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
	static DecodedFrames DecodeVidFrames(const VidRawData& vid, sg_sampler sampler);

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
		std::vector<SimguiUniqueImage> simguiImages;
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
	ImGui::Text("W: %i", self.anotherWidth);
	ImGui::Text("H: %i", self.anotherHeight);
	ImGui::Text("Z: %i", self.z_or_height);
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
	ImGui::Text("direction? % i", self.direction);
	ImGui::Text("z2 : % i", self.z);

	if (!self.vid) {
		ImGui::Text("Source nVid: %i", -self.dataSizeOrNvid);
		return;
	}

	ImGui::Text("frames size: %i", self.dataSizeOrNvid);
	ImGui::Text("Visual behavior: %x (%s)", self.visualBehavior, classifyVisualBehavior(self.visualBehavior));
	ImGui::Text("???: %i", self.hz7);
	ImGui::Text("numOfFrames: %i", self.numOfFrames);
	ImGui::Text("dataSize: %i", self.dataSize);
	ImGui::Text("imgWidth: %i", self.imgWidth);
	ImGui::Text("imgHeight: %i", self.imgHeight);
	

	if (!m_decodedFrames)
		return;

	static ImVec2 lastWindowSize = {100, 100};
	ImGui::SetNextWindowSize(lastWindowSize, ImGuiCond_Appearing);
	if (ImGui::Begin("Decompressed images", nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) {
		size_t imagesPerLine = std::max(1.0f, std::floor(ImGui::GetWindowWidth() / (self.imgWidth + 2.0f)));
		for (const auto& [index, image] : m_decodedFrames->simguiImages | std::views::enumerate) {
			ImGui::Image(simgui_imtextureid(image), {static_cast<float>(self.imgWidth), static_cast<float>(self.imgHeight)});

			if ((index+1) % imagesPerLine != 0)
				ImGui::SameLine();
		}

		lastWindowSize = ImGui::GetWindowSize();
	}
	ImGui::End();

}

VidsWindowViewModel::DecodedFrames VidsWindowViewModel::DecodeVidFrames(const VidRawData& vid, sg_sampler sampler) {
	auto images = vid.decode() | std::views::transform([&](const std::vector<RGBA8>& data) -> SgUniqueImage {
		return sg_image_desc{.type = SG_IMAGETYPE_2D,
			.width = static_cast<int>(vid.imgWidth),
			.height = static_cast<int>(vid.imgHeight),
			.num_slices = 1,
			.pixel_format = SG_PIXELFORMAT_RGBA8,
			.data = {{{{.ptr = data.data(), .size = data.size() * sizeof(RGBA8)}}}}};
	}) | std::ranges::to<std::vector>();

	auto simguiImages = images |
						std::views::transform([&](sg_image image) -> SimguiUniqueImage { return simgui_image_desc_t{.image = image, .sampler = sampler}; }) |
						std::ranges::to<std::vector>();

	return {
		.images = std::move(images),
		.simguiImages = std::move(simguiImages),
	};
}