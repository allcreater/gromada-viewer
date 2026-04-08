cmake_minimum_required (VERSION 3.30)

find_package(IMGUI REQUIRED CONFIG)
find_package(SFML COMPONENTS Graphics Window Audio System CONFIG REQUIRED)
find_package(ImGui-SFML CONFIG REQUIRED)

add_library(third_party_libs STATIC)
target_link_libraries(third_party_libs PUBLIC SFML::Graphics SFML::Window SFML::System ImGui-SFML::ImGui-SFML)
target_sources( third_party_libs PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS
        FILES
        "imgui_sfml_adapter.cppm"
)

find_package(nlohmann_json CONFIG REQUIRED)
add_library(nlohmann_json_adapter)
target_link_libraries(nlohmann_json_adapter PUBLIC nlohmann_json::nlohmann_json)
target_sources(nlohmann_json_adapter PUBLIC
  FILE_SET CXX_MODULES
  BASE_DIRS
  FILES
    "nlohmann_json_adapter.cppm"
)