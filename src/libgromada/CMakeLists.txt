cmake_minimum_required (VERSION 4.2)

add_library(libgromada STATIC
		"map_loader.cpp"
		"map_saver.cpp"
)

target_sources(libgromada
	PUBLIC
	FILE_SET modules
	TYPE CXX_MODULES
	FILES
		"actions.ixx"
		"game_resources.cppm"
		"graphics_decoder.cppm"
		"map.cppm"
		"terramorphing.cppm"
		"resource_reader.cppm"
		"resources.cppm"
		"resources_sound.cppm"
		"software_renderer.cppm"
		"visual_logic.cppm"
)

target_link_libraries(libgromada
	PUBLIC
		GromadaCore
)

set_target_properties(libgromada PROPERTIES
		CXX_STANDARD 23
		CXX_STANDARD_REQUIRED ON
		CXX_MODULE_STD ON
		CXX_EXTENSIONS OFF
)
