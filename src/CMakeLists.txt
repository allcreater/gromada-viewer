cmake_minimum_required (VERSION 4.2)

include(BuildInfo)

add_subdirectory(core)
add_subdirectory(libgromada)

add_executable (GromadaEditor
		"main.cpp"
)

 target_sources(GromadaEditor PRIVATE
	FILE_SET modules
	TYPE CXX_MODULES
	FILES
		"engine/level_renderer.cppm"
		"engine/objects_view.cppm"
		"engine/world_components.cppm"
		"engine/audio_engine.cppm"
		"view_models/vids.cpp"
		"view_models/map.cpp"
		"view_models/map_selector.cpp"
		"view_models/map_properties.cpp"
		"view_models/sounds.cpp"
		"application_dialogs.cpp"
		"application.cpp"
		"application_model.cpp"
		"application_view_model.cpp"
		"data_exporters.cppm"
		"framebuffer.cppm"
		"imgui_utils.cpp"
 )

#find_package(SFML REQUIRED)

target_link_libraries(GromadaEditor
	PRIVATE
		third_party_libs
		argparse::argparse
		flecs::flecs_static
		nlohmann_json_modules
		libgromada
)

target_compile_definitions(GromadaEditor PUBLIC FLECS_CPP_NO_AUTO_REGISTRATION)

set_target_properties(GromadaEditor PROPERTIES
		CXX_STANDARD 23
		CXX_STANDARD_REQUIRED ON
		CXX_MODULE_STD ON
		CXX_EXTENSIONS OFF
)

setup_build_info(GromadaEditor)

if(EMSCRIPTEN)
	set_target_properties(GromadaEditor PROPERTIES SUFFIX ".html")
endif()
