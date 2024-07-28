cmake_minimum_required (VERSION 3.29)

find_package(IMGUI REQUIRED)

add_library(sokol)
target_include_directories(sokol PUBLIC sokol)
target_link_libraries(sokol PUBLIC imgui::imgui)
target_compile_definitions(sokol PRIVATE SOKOL_GLCORE)
target_sources( sokol PUBLIC
  FILE_SET CXX_MODULES
  BASE_DIRS
  FILES
	"sokol.cppm"
	"sokol-imgui.cppm"

)