cmake_minimum_required (VERSION 3.29)

add_executable (GromadaEditor
	"main.cpp"
 )


 target_sources(GromadaEditor PRIVATE 
	FILE_SET modules
	TYPE CXX_MODULES
	FILES
		"resource_reader.cpp"
		"application.cpp"
  "imgui_utils.cpp")

#find_package(SFML REQUIRED)
find_package(argparse CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)

target_link_libraries(GromadaEditor
	PRIVATE
		sokol
		argparse::argparse
		jsoncpp_lib
)


#add_subdirectory("data/shaders")
#add_dependencies(VulkanViewer app_shaders)

target_compile_features(GromadaEditor PRIVATE cxx_std_23 )
