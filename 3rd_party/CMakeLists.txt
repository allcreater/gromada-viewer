cmake_minimum_required (VERSION 3.30)

find_package(IMGUI REQUIRED CONFIG)

add_library(third_party_libs)
target_include_directories(third_party_libs PUBLIC sokol)
target_link_libraries(third_party_libs PUBLIC imgui::imgui)
target_compile_definitions(third_party_libs PUBLIC SOKOL_GLCORE)
target_sources( third_party_libs PUBLIC
	"sokol_impl.cpp"
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