cmake_minimum_required (VERSION 4.2)

# Foundation modules with no dependencies on the rest of the codebase,
# shared between libgromada and GromadaEditor.
add_library(GromadaCore STATIC)

target_sources(GromadaCore
	PUBLIC
	FILE_SET modules
	TYPE CXX_MODULES
	FILES
		"bounding_box.cppm"
		"cp866.cppm"
		"utils.cppm"
		"utils_flags.cppm"
		"utils_streamreader.cppm"
)

set_target_properties(GromadaCore PROPERTIES
		CXX_STANDARD 23
		CXX_STANDARD_REQUIRED ON
		CXX_MODULE_STD ON
		CXX_EXTENSIONS OFF
)
