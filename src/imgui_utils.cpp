module;
#include <imgui.h>
#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_glue.h>
#include <util/sokol_imgui.h>

#include <cassert>

#if __INTELLISENSE__
#include <span>
#endif

export module imgui_utils;

import std;

namespace MyImUtils {

export template<typename T, typename Fn = std::identity>
bool ListBox(const char* label, int* current_item, std::span<T> items, Fn textToStr = {}, ImVec2 size = { -FLT_MIN, -FLT_MIN })
{
    const auto items_count = items.size();
    //using namespace ImGui
    //ImGuiContext& g = *GImGui;

    //// Calculate size from "height_in_items"
    //if (height_in_items < 0)
    //    height_in_items = ImMin(items_count, 7);
    //float height_in_items_f = height_in_items + 0.25f;
    //ImVec2 size(0.0f, ImTrunc(GetTextLineHeightWithSpacing() * height_in_items_f + g.Style.FramePadding.y * 2.0f));

    if (!ImGui::BeginListBox(label, size))
        return false;

    // Assume all items have even height (= 1 line of text). If you need items of different height,
    // you can create a custom version of ListBox() in your code without using the clipper.
    bool value_changed = false;
    ImGuiListClipper clipper;
    clipper.Begin(items_count, ImGui::GetTextLineHeightWithSpacing()); // We know exactly our line height here so we pass it as a minor optimization, but generally you don't need to.
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            const char* item_text = textToStr(items[i]);
            if (item_text == nullptr)
                item_text = "*Unknown item*";

            ImGui::PushID(i);
            const bool item_selected = (i == *current_item);
            if (ImGui::Selectable(item_text, item_selected))
            {
                *current_item = i;
                value_changed = true;
            }
            if (item_selected)
                ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
    ImGui::EndListBox();

    //if (value_changed)
    //    MarkItemEdited(g.LastItemData.ID);

    return value_changed;
}

template <typename T> const char* payloadTypeName() noexcept { 
	static std::array<char, sizeof ImGuiPayload::DataType> typeName = [] {
		const char* stdName = typeid(T).name();

		std::array<char, sizeof ImGuiPayload::DataType> nameBuffer;
		if (std::string_view(stdName).size() > nameBuffer.size()) {
			std::format_to_n(nameBuffer.data(), nameBuffer.size(), "hash_{}", typeid(T).hash_code());
		}
		else {
			std::strncpy(nameBuffer.data(), stdName, nameBuffer.size() - 1);
        }

		return nameBuffer;
	}();

	return typeName.data();
}

export template<typename T> 
    requires std::is_trivially_copyable_v<T>
bool SetDragDropPayload(const T& object) {
	return ImGui::SetDragDropPayload(payloadTypeName<T>(), &object, sizeof(T));
}

export template <typename T>
	requires std::is_trivially_copyable_v<T>
std::optional<std::pair<T, const ImGuiPayload*>> AcceptDragDropPayload(ImGuiDragDropFlags flags = {}) {
	const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadTypeName<T>(), flags);
	if (!payload)
		return std::nullopt;

    T object;
	assert(sizeof (T) == payload->DataSize);
	std::memcpy(&object, payload->Data, sizeof (T));
	return std::pair{std::move(object), payload};
}

}
