cmake_minimum_required (VERSION 3.30)

add_executable (GromadaEditor
	"main.cpp"
 )


 target_sources(GromadaEditor PRIVATE 
	FILE_SET modules
	TYPE CXX_MODULES
	FILES
		"resource_reader.cpp"
		"application.cpp"
		"imgui_utils.cpp"
		"resources.cpp"
		"view_models/vids.cpp"
		"view_models/map.cpp"
		"view_models/map_selector.cpp"
		"application_view_model.cpp"
		"application_model.cpp"
		"utils.cppm"
		"data_exporters.cppm"
 )

#find_package(SFML REQUIRED)
find_package(argparse CONFIG REQUIRED)

target_link_libraries(GromadaEditor
	PRIVATE
		third_party_libs
		argparse::argparse
		nlohmann_json_adapter
)


#add_subdirectory("data/shaders")
#add_dependencies(VulkanViewer app_shaders)

target_compile_features(GromadaEditor PRIVATE cxx_std_23 )
