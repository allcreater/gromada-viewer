cmake_minimum_required (VERSION 3.30)

include(FetchLibraries.cmake)

add_library(third_party_libs STATIC)
target_link_libraries(third_party_libs PUBLIC SFML::Graphics SFML::Window SFML::System ImGui-SFML::ImGui-SFML glm::glm)
target_sources( third_party_libs PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS
        FILES
        "imgui_sfml_adapter.cppm"
)

add_library(nlohmann_json_adapter)
target_link_libraries(nlohmann_json_adapter PUBLIC nlohmann_json::nlohmann_json)
target_sources(nlohmann_json_adapter PUBLIC
  FILE_SET CXX_MODULES
  BASE_DIRS
  FILES
    "nlohmann_json_adapter.cppm"
)