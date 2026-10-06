module;
#include <flecs.h>
#include <imgui.h>
#include <imgui_internal.h>

export module application.view_model:favorites;

import std;
import application.model;

export class FavoritesViewModel {
public:
	explicit FavoritesViewModel(Model& model)
		: m_model{model} {
		registerSettingsHandler();
	}

	void updateUI() {
		auto& groups = favorites().groups;
		std::optional<std::uint32_t> groupToDelete;
		for (auto& group : groups) {
			if (group.visible && groupWindow(group))
				groupToDelete = group.id;
		}

		if (groupToDelete) {
			std::erase_if(groups, [&](const auto& group) { return group.id == *groupToDelete; });
			ImGui::MarkIniSettingsDirty();
		}
	}

	void onMenu() {
		for (auto& group : favorites().groups) {
			ImGui::PushID(group.id);
			if (ImGui::MenuItem(group.name.c_str(), nullptr, &group.visible))
				ImGui::MarkIniSettingsDirty();
			ImGui::PopID();
		}

		if (!favorites().groups.empty())
			ImGui::Separator();

		newGroupMenu();
	}

	void vidContextMenu(VidRef vid) {
		ImGui::SeparatorText("Favorites");
		for (auto& group : favorites().groups) {
			ImGui::PushID(group.id);
			const bool contains = std::ranges::contains(group.vids, vid);
			if (ImGui::MenuItem(group.name.c_str(), nullptr, contains)) {
				if (contains)
					std::erase(group.vids, vid);
				else
					group.vids.push_back(vid);
				ImGui::MarkIniSettingsDirty();
			}
			ImGui::PopID();
		}

		if (auto* group = newGroupMenu())
			group->vids.push_back(vid);
	}

private:
	using Group = FavoriteVids::Group;

	FavoriteVids& favorites() { return m_model.get_mut<FavoriteVids>(); }

	// returns true if deletion requested
	bool groupWindow(Group& group) {
		bool deleteRequested = false;

		ImGui::SetNextWindowSize({220, 300}, ImGuiCond_FirstUseEver);
		const auto title = std::format("{}###favorites_{}", group.name, group.id);
		if (ImGui::Begin(title.c_str(), &group.visible)) {
			if (ImGui::BeginListBox("##vids", {-FLT_MIN, -FLT_MIN})) {
				const auto selectedVid = m_model.get<GlobalEditorState>().selectedNvid;
				std::optional<VidRef> vidToRemove;
				for (const auto vid : group.vids) {
					ImGui::PushID(vid.nvid());
					if (ImGui::Selectable(std::format("{:>4}  {}", vid.nvid(), vid->getName()).c_str(), vid == selectedVid))
						selectVid(vid);

					if (ImGui::BeginPopupContextItem()) {
						if (ImGui::MenuItem("Remove"))
							vidToRemove = vid;
						ImGui::EndPopup();
					}
					ImGui::PopID();
				}

				if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
					if (ImGui::BeginMenu("Rename")) {
						if (auto name = nameInput(group.name)) {
							group.name = std::move(*name);
							ImGui::MarkIniSettingsDirty();
							ImGui::CloseCurrentPopup();
						}
						ImGui::EndMenu();
					}
					deleteRequested = ImGui::MenuItem("Delete group");
					ImGui::EndPopup();
				}
				ImGui::EndListBox();

				if (vidToRemove) {
					std::erase(group.vids, *vidToRemove);
					ImGui::MarkIniSettingsDirty();
				}
			}
		}
		ImGui::End();

		if (!group.visible)
			ImGui::MarkIniSettingsDirty();

		return deleteRequested;
	}

	Group* newGroupMenu() {
		Group* newGroup = nullptr;
		if (ImGui::BeginMenu("New group")) {
			if (auto name = nameInput({})) {
				auto& favs = favorites();
				newGroup = &favs.groups.emplace_back(favs.nextGroupId++, std::move(*name));
				ImGui::MarkIniSettingsDirty();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndMenu();
		}
		return newGroup;
	}

	std::optional<std::string> nameInput(std::string_view initialName) {
		if (ImGui::IsWindowAppearing()) {
			m_nameBuffer = {};
			initialName.copy(m_nameBuffer.data(), m_nameBuffer.size() - 1);
			ImGui::SetKeyboardFocusHere();
		}

		if (ImGui::InputTextWithHint("##name", "Group name", m_nameBuffer.data(), m_nameBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
			if (std::string name{m_nameBuffer.data()}; !name.empty())
				return name;
		}
		return std::nullopt;
	}

	void selectVid(VidRef vid) {
		auto& state = m_model.get_mut<GlobalEditorState>();
		state.selectedNvid = vid;
		state.state = PlacementState{};
		m_model.modified<GlobalEditorState>();
		flushDerivedState(m_model);
	}

	// Model outlives the ImGui context, so it's safe to be used by the handler during the final ini saving
	void registerSettingsHandler() {
		ImGuiSettingsHandler handler;
		handler.TypeName = "FavoriteVids";
		handler.TypeHash = ImHashStr(handler.TypeName);
		handler.UserData = &m_model;
		handler.ReadInitFn = [](ImGuiContext*, ImGuiSettingsHandler* handler) {
			favoritesOf(handler) = {};
		};
		handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, const char* name) -> void* {
			auto& favs = favoritesOf(handler);
			return &favs.groups.emplace_back(favs.nextGroupId++, name);
		};
		handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, void* entry, const char* line) {
			readGroupLine(*static_cast<Group*>(entry), line, static_cast<Model*>(handler->UserData)->get<const GameResources>());
		};
		handler.ApplyAllFn = [](ImGuiContext*, ImGuiSettingsHandler* handler) {
			auto& favs = favoritesOf(handler);
			for (const auto& group : favs.groups)
				favs.nextGroupId = std::max(favs.nextGroupId, group.id + 1);
		};
		handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buffer) {
			for (const auto& group : favoritesOf(handler).groups) {
				const auto nvids = group.vids | std::views::transform([](VidRef vid) { return std::to_string(vid.nvid()); }) | std::views::join_with(',') |
								   std::ranges::to<std::string>();
				buffer->appendf("[%s][%s]\nId=%u\nVisible=%d\nVids=%s\n\n", handler->TypeName, group.name.c_str(), group.id, group.visible, nvids.c_str());
			}
		};
		ImGui::AddSettingsHandler(&handler);
	}

	static FavoriteVids& favoritesOf(ImGuiSettingsHandler* handler) { return static_cast<Model*>(handler->UserData)->get_mut<FavoriteVids>(); }

	static void readGroupLine(Group& group, std::string_view line, const GameResources& resources) {
		const auto separator = line.find('=');
		if (separator == std::string_view::npos)
			return;

		const auto key = line.substr(0, separator);
		const auto value = line.substr(separator + 1);
		if (key == "Id")
			std::from_chars(value.data(), value.data() + value.size(), group.id);
		else if (key == "Visible")
			group.visible = value != "0";
		else if (key == "Vids")
			group.vids = parseVids(value, resources);
	}

	static std::vector<VidRef> parseVids(std::string_view list, const GameResources& resources) {
		const auto vidsCount = static_cast<int>(resources.vids().size());
		return list | std::views::split(',') | std::views::transform([](auto token) {
				   int nvid = -1;
				   std::from_chars(token.data(), token.data() + token.size(), nvid);
				   return nvid;
			   }) |
			   std::views::filter([vidsCount](int nvid) { return nvid >= 0 && nvid < vidsCount; }) |
			   std::views::transform([&resources](int nvid) { return resources.getVid(nvid); }) | std::ranges::to<std::vector>();
	}

	Model& m_model;
	std::array<char, 64> m_nameBuffer{};
};
