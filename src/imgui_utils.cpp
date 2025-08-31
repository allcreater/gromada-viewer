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
bool ListBox(const char* label, int* current_item, std::span<const T> items, Fn itemToStr = {}, ImVec2 size = { -FLT_MIN, -FLT_MIN })
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
            const char* item_text = itemToStr(items[i]);
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

export template<typename T, typename Fn = std::identity>
bool ComboBox(const char* label, int* current_item, std::span<const T> items, Fn itemToStr = {}) {
    struct Context {
        std::span<const T> items;
        Fn itemToStr;
    };

    const auto getter = [](void* user_data, int idx) -> const char* {
        auto context = static_cast<Context*>(user_data);

        thread_local std::invoke_result_t<Fn, const T> item_text = {};
        item_text = context->itemToStr(context->items[idx]);

        if constexpr (std::is_convertible_v<decltype(item_text), const char*>) {
            return item_text;
        }
        else if constexpr (requires { std::as_const(item_text).c_str(); }) {
            static_assert(sizeof(*std::as_const(item_text).c_str()) == 1, "itemToStr must return multi-byte string");
            return reinterpret_cast<const char*>(std::as_const(item_text).c_str());
        }
        else {
            static_assert(false, "itemToStr must return const char* or string-like type");
            return "*Unknown item*";
        }
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

}
