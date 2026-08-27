module;
#include <imgui.h>
#include <cassert>

export module imgui_utils;

import std;

namespace MyImUtils {

namespace detail {

template <typename Str>
const char* c_str(const Str& value) noexcept {
    if constexpr (std::convertible_to<Str, const char*>) {
        return value;
    } else if constexpr (requires { { value.c_str() } -> std::same_as<const char8_t*>; }) {
        return reinterpret_cast<const char*>(value.c_str());
    } else {
        return value.c_str();
    }
}

template <typename Str>
bool SelectableWithText(Str&& text, bool selected) {
    const char* s = c_str(text);
    return ImGui::Selectable(s ? s : "*Unknown item*", selected);
}

}

// --- Interface ---

template <typename T, typename ItemType>
concept ListItemCallback = requires(T fn, const ItemType& item, bool item_selected)
{
    { fn(item, item_selected) } -> std::same_as<bool>;
};

export template <typename ItemType, typename Fn = std::identity>
    requires requires (ItemType item, Fn fn) {  { detail::c_str(std::invoke(fn, item)) } -> std::same_as<const char*>; }
ListItemCallback<ItemType> auto MakeSelectableCallback(Fn item_to_str) {
    return [item_to_str = std::move(item_to_str)](const ItemType& item, bool item_selected) mutable {
        return detail::SelectableWithText(std::invoke(item_to_str, item), item_selected);
    };
}

export template<typename ItemType, ListItemCallback<ItemType> Fn>
bool ListBox(const char* label, int* current_item, std::span<ItemType> items, Fn item_callback, ImVec2 size = { -FLT_MIN, -FLT_MIN });

export template<typename T, typename Fn = std::identity>
bool ComboBox(const char* label, int* current_item, std::span<T> items, Fn itemToStr = {});

template <typename T> const char* payloadTypeName() noexcept;

export template<typename T>
    requires std::is_trivially_copyable_v<T>
bool SetDragDropPayload(const T& object);

export template <typename T>
	requires std::is_trivially_copyable_v<T>
std::optional<std::pair<T, const ImGuiPayload*>> AcceptDragDropPayload(ImGuiDragDropFlags flags = {});

// Opens `name` as a modal popup once when `shouldOpen` is true (resetting the flag), centers it on
// first appearance, and forwards to BeginPopupModal. Mirrors BeginPopupModal's contract: only call
// EndPopup() if this returns true.
export bool BeginModalPopup(const char* name, bool& shouldOpen, ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

// std::format-based alternative to ImGui::Text(fmt, ...)
export template <typename... Args>
void Text(std::format_string<Args...> fmt, Args&&... args);

export void ToggleButton(const char* label, bool active, auto&& onClick);



//
// --- Implementation ---
//
template<typename ItemType, ListItemCallback<ItemType> Fn>
bool ListBox(const char* label, int* current_item, std::span<ItemType> items, Fn item_callback, ImVec2 size)
{
    const auto items_count = items.size();

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

template<typename T, typename Fn>
bool ComboBox(const char* label, int* current_item, std::span<T> items, Fn itemToStr) {
    struct Context {
        std::span<const T> items;
        Fn itemToStr;
    };

    const auto getter = [](void* user_data, int idx) -> const char* {
        auto context = static_cast<Context*>(user_data);

        thread_local std::invoke_result_t<Fn, const T> item_text = {};
        item_text = context->itemToStr(context->items[idx]);

        return detail::c_str(item_text);
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

template<typename T>
    requires std::is_trivially_copyable_v<T>
bool SetDragDropPayload(const T& object) {
	return ImGui::SetDragDropPayload(payloadTypeName<T>(), &object, sizeof(T));
}

template <typename T>
	requires std::is_trivially_copyable_v<T>
std::optional<std::pair<T, const ImGuiPayload*>> AcceptDragDropPayload(ImGuiDragDropFlags flags) {
	const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadTypeName<T>(), flags);
	if (!payload)
		return std::nullopt;

    T object;
	assert(sizeof (T) == payload->DataSize);
	std::memcpy(&object, payload->Data, sizeof (T));
	return std::pair{std::move(object), payload};
}

bool BeginModalPopup(const char* name, bool& shouldOpen, ImGuiWindowFlags flags) {
    if (shouldOpen) {
        ImGui::OpenPopup(name);
        shouldOpen = false;
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    return ImGui::BeginPopupModal(name, nullptr, flags);
}

template <typename... Args>
void Text(std::format_string<Args...> fmt, Args&&... args) {
    static std::string buffer;
    buffer.clear();
    std::format_to(std::back_inserter(buffer), fmt, std::forward<Args>(args)...);
    ImGui::TextUnformatted(buffer.data(), buffer.data() + buffer.size());
}

void ToggleButton(const char* label, bool active, auto&& onClick) {
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
