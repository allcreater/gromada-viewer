module;
#include <flecs.h>
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
import Gromada.VisualLogic;
import framebuffer;
import engine.audio;

import utils;

auto makeComparator(const ImGuiTableSortSpecs& sortSpecs) {
	constexpr static auto extractGraphicsGormat = [](const Vid& vid) {
		const auto* graphics = std::get_if<Vid::Graphics>(&(vid.graphicsData));
		return graphics ? graphics->get()->dataFormat : -1;
	};

	const std::array comparators{
		+[](const Vid& a, const Vid& b) { return &a <=> &b; }, // we are sure they are in the same array, so that it's safe
		+[](const Vid& a, const Vid& b) { return a.name <=> b.name; },
		+[](const Vid& a, const Vid& b) { return a.type <=> b.type; },
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
		: m_model{model} {

		m_model.observer<GlobalEditorState>()
			.event(flecs::OnSet)
			.each([this](flecs::entity _, const GlobalEditorState& state) {
				InvalidateSelection();
			});
	}

	void updateUI() {
		// By default, if we don't enable ScrollX the sizing policy for each column is "Stretch"
		// All columns maintain a sizing weight, and they will occupy all available width.
		static ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
									   ImGuiTableFlags_ContextMenuInBody;

		const auto prevSelectedSection = selectedSection();

	    ImGui::Checkbox("Show details", &m_showDetails);
		if (ImGui::BeginTable(
				"vids_list_table", 4, ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable)) {
			ImGui::TableSetupColumn("NVID", ImGuiTableColumnFlags_WidthFixed, 30.0f);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 100.0f);
			ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthStretch, 60.0f);
			ImGui::TableSetupColumn("G/F", ImGuiTableColumnFlags_WidthFixed, 30.0f);

			ImGui::TableSetupScrollFreeze(0, 1); // Make header always visible
			ImGui::TableHeadersRow();

			if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty) {
				std::ranges::sort(m_sortedVids, makeComparator(*specs));
				specs->SpecsDirty = false;
			}

			for (const auto& vid : m_sortedVids) {

				ImGui::TableNextColumn();
				bool isElementSelected = (selectedSection() == vid);

				if (ImGui::Selectable(std::to_string(vid.nvid()).c_str(), isElementSelected, ImGuiSelectableFlags_SpanAllColumns)) {
					selectedSection(vid);
				}
				if (isElementSelected) {
					ImGui::SetItemDefaultFocus();
				    if (std::exchange(m_selecionInvalidated, false)) {
				        ImGui::SetScrollHereY(0.5f); // Scroll to the selected item
				    }
				}

				ImGui::TableNextColumn();
				MyImUtils::Text("{}", vid->getName());

				ImGui::TableNextColumn();
				MyImUtils::Text("{}", to_string(vid->type));

				ImGui::TableNextColumn();
			    MyImUtils::Text("{}", vid->graphics().dataFormat);

				ImGui::TableNextRow();
			}

			ImGui::EndTable();

		    if (prevSelectedSection != selectedSection()) {
		        InvalidateSelection();
		    }

		    if (m_showDetails && selectedSection()) {
		        ImGui::SetNextWindowPos({320, 20}, ImGuiCond_FirstUseEver);
		        if (ImGui::Begin("Vid details", &m_showDetails)) {
					VidUI(selectedSection());

					ImGui::SetNextWindowPos({10, 530}, ImGuiCond_FirstUseEver);
		            ImGui::SetNextWindowSize({300, 280}, ImGuiCond_FirstUseEver);
		            ShowFramesWindow(selectedSection());
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
	
	static std::vector<SgUniqueImageWithView> DecodeVidFrames(const VidGraphics& vid, sg_sampler sampler);

	VidRef selectedSection() {
		return m_model.get<GlobalEditorState>().selectedNvid;
	}

	void selectedSection(VidRef vid) {
		m_model.get_mut<GlobalEditorState>().selectedNvid = vid;
		m_model.modified<GlobalEditorState>();
		flushDerivedState(m_model);
	}

private:
	Model& m_model;
    bool m_showDetails = false;
	bool m_showFrameNumbers = false;
	std::vector<VidRef> m_sortedVids{std::from_range, m_model.get<const GameResources>().vidRefs()};
    bool m_selecionInvalidated = true;
	SgUniqueSampler m_guiImagesSampler{sg_sampler_desc{
		.min_filter = SG_FILTER_LINEAR,
		.mag_filter = SG_FILTER_LINEAR,
		.wrap_u = SG_WRAP_REPEAT,
		.wrap_v = SG_WRAP_REPEAT,
		.wrap_w = SG_WRAP_REPEAT,
	}};

	std::vector<SgUniqueImageWithView> m_decodedFrames;
	struct FramesWindowState {
		int direction = 0;
		bool showAnimation = false;
		Stopwatch stopwatch;
		int frameNumber = 0;
	} m_framesWindowState;
};

namespace {
	constexpr static std::array<const char*, 16> actionNames = {"Stand", "Build", "Go", "Start move", "L Rotate", "R Rotate", "Open", "Close", "Fight", "Salut",
	"Stand open", "Load", "Unload", "Wound", "Birth", "Death"};
}


void VidsWindowViewModel::VidUI(const Vid& self) {
	const auto& resources = m_model.get<const GameResources>();

    auto linkToId = [&, guiId = 0](int id, auto&& onClick) mutable {
        if (id) {
            std::array<char, 32> buffer {0};
            std::format_to_n(buffer.data(), buffer.size(), "{}", id);
            ImGui::PushID(guiId++);
            if (ImGui::TextLink(buffer.data()) ) {
				std::invoke(onClick, id);

            }
            ImGui::PopID();
        }
        else {
            ImGui::Text("n/a");
        }
    };

	auto linkToNvidControl = std::bind_back(linkToId, [this, &resources](int nvid) {
		selectedSection(resources.getVid(std::abs(nvid)));
		InvalidateSelection();
	});

	auto linkToSound = std::bind_back(linkToId, [&resources = m_model.get<GameResources>(), &soundEngine = m_model.get_mut<AudioEngine>()](int nsfx) {
		soundEngine.playSound(resources.sounds()[nsfx]);
	});

    MyImUtils::Text("{}", self.getName());
    MyImUtils::Text("unitType: {} ", to_string(self.category));
    MyImUtils::Text("Class: {} ", to_string(self.type));
    MyImUtils::Text("Flags: {}", to_string(Flags{self.flags}));
    MyImUtils::Text("Collision mask: {:x}", self.collisionMask);
    MyImUtils::Text("Sizes (W,H,Z): {} {} {}", self.sizeX, self.sizeY, self.sizeZ);
    MyImUtils::Text("max HP: {}", self.maxHP);
    MyImUtils::Text("visibility radius: {}", self.visibilityRadius);

    MyImUtils::Text("Speed: {} {}", self.speedX, self.speedY);
    MyImUtils::Text("Acceleration: {}", self.acceleration);
    MyImUtils::Text("Rotation period: {}", self.rotationPeriod);

    MyImUtils::Text("Army: {}", self.army);

	MyImUtils::Text("Weapon #{}:", self.someWeaponIndex);
	if (self.someWeaponIndex <= 0) {
		ImGui::SameLine();
		ImGui::Text("N/A");
	} else if (ImGui::BeginChild("Weapon", ImVec2(0, 0),  ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders)) {
		const auto& weapon = resources.weapons()[self.someWeaponIndex];
	    MyImUtils::Text("Targets: {}", to_string(Flags{weapon.targetCategory}));
		MyImUtils::Text("Flags: {:x}", weapon.flags);
		MyImUtils::Text("Range: {}", weapon.weaponRange);
		MyImUtils::Text("Scatter: {}", weapon.scatter);
		MyImUtils::Text("Cooldown: {}", weapon.cooldown);
		ImGui::EndChild();
	}

    MyImUtils::Text("???: {}", self.unused2);
    MyImUtils::Text("Damage radius: {}", self.deathDamageRadius);
    MyImUtils::Text("Damage: {}", self.deathDamage);
    MyImUtils::Text("Linked object offset (X,Y,Z): {} {} {}", self.linkX, self.linkY, self.linkZ);

    ImGui::Text("Linked nVid: "); ImGui::SameLine(); linkToNvidControl(self.linkedObjectVid);
    MyImUtils::Text("Directions count: {}", self.directionsCount);
    MyImUtils::Text("Z Layer: {}", self.z_layer);


    ImGui::Spacing();

    ImGui::BeginTable("Actions", 6);
    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 70.0f);
	ImGui::TableSetupColumn("frames count", ImGuiTableColumnFlags_WidthFixed, 60.0f);
	ImGui::TableSetupColumn("nsfx", ImGuiTableColumnFlags_WidthFixed, 50.0f);
	ImGui::TableSetupColumn("nvid", ImGuiTableColumnFlags_WidthFixed, 50.0f);
	ImGui::TableSetupColumn("count", ImGuiTableColumnFlags_WidthFixed, 30.0f);
	ImGui::TableSetupColumn("offset", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableHeadersRow();

    for (std::size_t i = 0; i < 16; ++i) {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(actionNames[i]);

        ImGui::TableNextColumn();
        MyImUtils::Text("{}", self.animationLengths[i]);

        ImGui::TableNextColumn();
		linkToSound(self.nsfx[i]);

        ImGui::TableNextColumn();
        linkToNvidControl(self.childNvid[i]);

        ImGui::TableNextColumn();
        MyImUtils::Text("{}", self.childrenCount[i]);

        ImGui::TableNextColumn();
        MyImUtils::Text("{} {} {}", self.childrenOffsets[0][i], self.childrenOffsets[1][i], self.childrenOffsets[2][i]);

        ImGui::TableNextRow();
    }

    ImGui::EndTable();

    ImGui::Spacing();
    std::visit(overloaded{
                   [](std::int32_t arg) { MyImUtils::Text("Source nVid: {}", arg); },
                   [&self](const Vid::Graphics& arg) {
                       MyImUtils::Text("frames size: {}", self.dataSizeOrNvid);
                       MyImUtils::Text("data format: {:x}", arg->dataFormat);
                       MyImUtils::Text("frame duration: {}ms ({} FPS)", arg->frameDuration, 1000 / arg->frameDuration);
                       MyImUtils::Text("numOfFrames: {}", arg->numOfFrames);
                       MyImUtils::Text("dataSize: {}", arg->dataSize);
                       MyImUtils::Text("width: {}", arg->width);
                       MyImUtils::Text("height: {}", arg->height);
                   },
               },
        self.graphicsData);
}

void VidsWindowViewModel::ShowFramesWindow(const Vid& self) {
	if (m_decodedFrames.empty()) {
	    m_decodedFrames = DecodeVidFrames(selectedSection()->graphics(), m_guiImagesSampler);
	    if (m_decodedFrames.empty())
	        return;
	}

	const auto framesData = std::get_if<Vid::Graphics>(&self.graphicsData);
	const auto ShowFrame = [&](size_t index) {
		ImGui::Image(simgui_imtextureid(m_decodedFrames[index]), {static_cast<float>((*framesData)->width), static_cast<float>((*framesData)->height)});
	};


	if (ImGui::Begin("Decompressed images", nullptr, ImGuiWindowFlags_NoFocusOnAppearing)) {
		ImGui::BeginTabBar( "FramesTabBar", ImGuiTabBarFlags_None);
		if (ImGui::BeginTabItem( "All frames" )) {
			ImGui::Checkbox( "Show numbers", &m_showFrameNumbers);
			std::size_t imagesPerLine = std::max(1.0f, std::floor(ImGui::GetContentRegionAvail().x / (*framesData)->width));
			for (int index = 0; index < m_decodedFrames.size(); ++index) {
				auto pos = ImGui::GetCursorScreenPos();
				ShowFrame(index);
				if ((index+1) % imagesPerLine != 0) {
					ImGui::SameLine();
				}

				if (m_showFrameNumbers) {
					ImGui::SetCursorScreenPos(std::exchange( pos, ImGui::GetCursorScreenPos() ));
					MyImUtils::Text("{}", index + 1);
					ImGui::SetCursorScreenPos(pos);
				}
			}

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem( "By action" )) {
			if (self.directionsCount > 1) {
				ImGui::SliderInt("Direction", &m_framesWindowState.direction, 0, self.directionsCount-1);
			} else {
				m_framesWindowState.direction = 0;
			}

			ImGui::Checkbox("Show animation", &m_framesWindowState.showAnimation);

			if (ImGui::BeginChild( "FramesChild", {0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {
				m_framesWindowState.frameNumber += m_framesWindowState.stopwatch.advance(ImGui::GetIO().DeltaTime, self.graphics().frameDuration * 0.001f);

				for (std::size_t i = 0; i < 16; ++i) {
					const auto frameRange = getAnimationFrameRangeDirIndex(self, static_cast<Action>(i), m_framesWindowState.direction);
					if (!frameRange)
						continue;

					ImGui::TextUnformatted(actionNames[i]);
					ImGui::NewLine();
					if (m_framesWindowState.showAnimation) {
						ShowFrame(frameRange->first + m_framesWindowState.frameNumber % (frameRange->second - frameRange->first + 1));
					} else {
						for (int index = frameRange->first; index <= frameRange->second; ++index) {
							ImGui::SameLine();
							ShowFrame(index);
						}
					}
				}
			}
			ImGui::EndChild();

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
	ImGui::Dummy({});
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

std::vector<SgUniqueImageWithView> VidsWindowViewModel::DecodeVidFrames(const VidGraphics& vid, sg_sampler sampler) {
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