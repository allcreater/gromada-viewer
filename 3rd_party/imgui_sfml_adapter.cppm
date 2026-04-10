module;
#include <imgui.h>
#include <SFML/Graphics.hpp>
#include <imgui-SFML.h>

export module imgui_sfml_adapter;

export {
    namespace ImGui::SFML {
        using ::ImGui::SFML::Init;
        using ::ImGui::SFML::SetCurrentWindow;
        using ::ImGui::SFML::ProcessEvent;
        using ::ImGui::SFML::Update;
        using ::ImGui::SFML::Render;
        using ::ImGui::SFML::Shutdown;
        using ::ImGui::SFML::UpdateFontTexture;
        using ::ImGui::SFML::GetFontTexture;

        // Custom additions
        [[nodiscard]] ImTextureID GetImguiTexture(const sf::Texture& texture) {
            return ImTextureID {texture.getNativeHandle()};
        }
    }

}