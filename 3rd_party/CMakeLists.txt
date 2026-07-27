cmake_minimum_required (VERSION 4.2)

set(CMAKE_CXX_MODULE_STD OFF)
set(CMAKE_CXX_STANDARD 23)

include(FetchLibraries.cmake)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
set_target_properties(imgui PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ΟΝ
        CXX_MODULE_STD OFF
        CXX_EXTENSIONS OFF
)

add_library(third_party_libs)
target_include_directories(third_party_libs PUBLIC ${sokol_SOURCE_DIR} ${miniaudio_SOURCE_DIR})
target_link_libraries(third_party_libs PUBLIC imgui glm::glm)
target_compile_definitions(third_party_libs PUBLIC SOKOL_GLCORE)
set_target_properties(third_party_libs PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ΟΝ
        CXX_MODULE_STD ON
        CXX_EXTENSIONS OFF
)
target_sources( third_party_libs PUBLIC
    "sokol_impl.cpp"
    "miniaudio_impl.cpp"
    "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
    PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS
    FILES
    "sokol_helpers.cppm"
)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)

    find_library(LIB_X11 X11 REQUIRED)
    find_library(LIB_Xi Xi REQUIRED)
    find_library(LIB_Xcursor Xcursor REQUIRED)
    find_library(LIB_EGL EGL REQUIRED)
    find_library(LIB_GL GL REQUIRED)
    find_library(LIB_dl dl REQUIRED)
    find_library(LIB_m m REQUIRED)
    target_link_libraries(third_party_libs PRIVATE ${LIB_X11} ${LIB_Xi} ${LIB_Xcursor} ${LIB_EGL} ${LIB_GL} ${LIB_dl} ${LIB_m} Threads::Threads)
endif()


add_library(nlohmann_json_adapter)
target_link_libraries(nlohmann_json_adapter PUBLIC nlohmann_json::nlohmann_json)
set_target_properties(nlohmann_json_adapter PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ΟΝ
        CXX_MODULE_STD ON
        CXX_EXTENSIONS OFF
)
target_sources(nlohmann_json_adapter PUBLIC
  FILE_SET CXX_MODULES
  BASE_DIRS
  FILES
    "nlohmann_json_adapter.cppm"
)