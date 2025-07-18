cmake_minimum_required (VERSION 3.30)

add_executable (GromadaEditor
	"main.cpp"
)


 target_sources(GromadaEditor PRIVATE 
	FILE_SET modules
	TYPE CXX_MODULES
	FILES
		"engine/bounding_box.cppm"
		"engine/level_renderer.cppm"
		"engine/objects_view.cppm"
		"engine/world_components.cppm"
	 	"gromada/actions.ixx"
		"gromada/data_exporters.cppm"
		"gromada/game_resources.cppm"
		"gromada/resource_reader.cpp"
		"gromada/resources.cpp"
		"gromada/software_renderer.cppm"
		"gromada/visual_logic.cpp"
		"view_models/vids.cpp"
		"view_models/map.cpp"
		"view_models/map_selector.cpp"
		"application.cpp"
		"application_model.cpp"
		"application_view_model.cpp"
 		"cp866.cppm"
		"framebuffer.cppm"
		"imgui_utils.cpp"
		"utils.cppm"
 )

#find_package(SFML REQUIRED)
find_package(argparse CONFIG REQUIRED)
find_package(flecs CONFIG REQUIRED)

target_link_libraries(GromadaEditor
	PRIVATE
		third_party_libs
		argparse::argparse
		$<IF:$<TARGET_EXISTS:flecs::flecs>,flecs::flecs,flecs::flecs_static>
		nlohmann_json_adapter
)


#add_subdirectory("data/shaders")
#add_dependencies(VulkanViewer app_shaders)

target_compile_definitions(GromadaEditor PUBLIC FLECS_CPP_NO_AUTO_REGISTRATION)

target_compile_features(GromadaEditor PRIVATE cxx_std_23 )
