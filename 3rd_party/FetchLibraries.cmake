cmake_minimum_required (VERSION 3.30)

include(FetchContent)

# Fetch third-party dependencies
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.7
)

FetchContent_Declare(
        sokol
        GIT_REPOSITORY https://github.com/floooh/sokol.git
        GIT_TAG d1bd35fbbec70ef91f91fe041d68731da8a15586 # from 08.04.2026
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
set(GLM_ENABLE_CXX_20 ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
        miniaudio
        GIT_REPOSITORY https://github.com/mackron/miniaudio.git
        GIT_TAG 0.11.25
)

# Make the rest dependencies available
FetchContent_MakeAvailable(imgui sokol nlohmann_json argparse flecs miniaudio)