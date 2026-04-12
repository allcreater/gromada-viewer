cmake_minimum_required (VERSION 3.30)

include(FetchContent)

# Fetch third-party dependencies
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.9b #v1.92.7
)

FetchContent_Declare(
        sfml
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 3.0.2
)

FetchContent_Declare(
        imgui-sfml
        GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
        GIT_TAG v3.0
)

FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
)

FetchContent_Declare(
        argparse
        GIT_REPOSITORY https://github.com/p-ranav/argparse.git
        GIT_TAG v2.9
)

FetchContent_Declare(
        flecs
        GIT_REPOSITORY https://github.com/SanderMertens/flecs.git
        GIT_TAG v4.1.5
)

FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.3
)

# Configure them
FetchContent_MakeAvailable(imgui)

set(SFML_BUILD_AUDIO OFF CACHE BOOL "SFML: Build audio" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "SFML: Build network" FORCE)
FetchContent_MakeAvailable(sfml)

set(IMGUI_DIR ${imgui_SOURCE_DIR} CACHE PATH "" FORCE)
set(IMGUI_SFML_FIND_SFML OFF CACHE BOOL "IMGUI-SFML: Use find_package to find SFML" FORCE)
set(IMGUI_SFML_IMGUI_DEMO OFF CACHE BOOL "IMGUI-SFML: Build imgui_demo.cpp" FORCE)
FetchContent_MakeAvailable(imgui-sfml)

set(GLM_ENABLE_CXX_20 ON CACHE BOOL "" FORCE)

# Make the rest dependencies available
FetchContent_MakeAvailable(nlohmann_json argparse flecs glm)