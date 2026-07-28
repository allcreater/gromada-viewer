cmake_minimum_required (VERSION 4.2)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_MODULE_STD ON)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4.0")
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "4.3.0")
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "4.1.0")
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
else()
    message("CMake version is too old to support C++ modules. Please update CMake to at least 4.2.0")
endif()

project(GromadaEditor
    VERSION 0.0.1
    LANGUAGES CXX
)

# Seems that some compilers wants to have CMAKE_CXX_MODULE_STD enabled before the project declaration, let's disable it back
set(CMAKE_CXX_MODULE_STD OFF)

message("Cmake version is ${CMAKE_VERSION}, standard library is ${CMAKE_CXX_STANDARD_LIBRARY}")

set(GVIEWER_RENDER_BACKEND "GLCORE" CACHE STRING "Rendering backend")
set_property(CACHE GVIEWER_RENDER_BACKEND PROPERTY STRINGS GLCORE GLES3 D3D11 METAL WGPU VULKAN)

add_subdirectory(3rd_party/)
add_subdirectory(src/)