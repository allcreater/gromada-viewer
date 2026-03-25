module;
#include <imgui.h>
#include <sokol_gfx.h>
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_glue.h>
#include <util/sokol_imgui.h>

#include <cassert>

export module imgui_utils;

import std;

namespace MyImUtils {

constexpr const char* c_str(const char* c_string) noexcept { return c_string; }
const char* c_str(const std::string& string) { return string.c_str(); }
const char* c_str(const std::u8string& string) { return reinterpret_cast<const char*>(string.c_str()); }

template <typename T, typename ItemType>
concept ListItemCallback = requires(T fn, const ItemType& item, bool item_selected)
{
    { fn(item, item_selected) } -> std::same_as<bool>;
};

export template <typename ItemType, typename Fn = std::identity>
    requires requires (ItemType item, Fn fn) {  { c_str(std::invoke(fn, item)) } -> std::same_as<const char*>; }
ListItemCallback<ItemType> auto MakeSelectableCallback(Fn item_to_str) {
    return [item_to_str = std::move(item_to_str)](const ItemType& item, bool item_selected) mutable {
        auto&& item_text = std::invoke(item_to_str, item);
        const char* text_c_str = c_str(item_text);
        if (text_c_str == nullptr)
            text_c_str = "*Unknown item*";

        return ImGui::Selectable(text_c_str, item_selected);
    };
}

export template<typename ItemType, ListItemCallback<ItemType> Fn>
bool ListBox(const char* label, int* current_item, std::span<ItemType> items, Fn item_callback, ImVec2 size = { -FLT_MIN, -FLT_MIN })
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
#ifndef DONT_USE_CLIPPER
    ImGuiListClipper clipper;
    clipper.Begin(items_count);
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
#else
    for (int i = 0; i < items_count; i++)
#endif
        {
            ImGui::PushID(i);
            const bool item_selected = (i == *current_item);
            if (item_callback(items[i], item_selected))
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

export template<typename T, typename Fn = std::identity>
bool ComboBox(const char* label, int* current_item, std::span<T> items, Fn itemToStr = {}) {
    struct Context {
        std::span<const T> items;
        Fn itemToStr;
    };

    const auto getter = [](void* user_data, int idx) -> const char* {
        auto context = static_cast<Context*>(user_data);

        thread_local std::invoke_result_t<Fn, const T> item_text = {};
        item_text = context->itemToStr(context->items[idx]);

        return c_str(item_text);
    };

    Context ctx{items, std::move(itemToStr)};
    return ImGui::Combo(label, current_item, getter, &ctx, items.size());
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

export void ToggleButton (const char* label, bool active, auto&& onClick) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    if (ImGui::Button(label)) {
        onClick();
    }
    if (active) {
        ImGui::PopStyleColor(2);
    }
}

}
