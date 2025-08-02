# gromada-viewer
A small program written in C++ 23 to view most game resources from the old good game "Gromada" developed by Buka Entertainment (1999)

# Usage
Just put the binaries to the root game directory and run the program.

# Features
* Interactive view of "Vids" database (game object properties and graphics) with all corresponding graphics frames
* Map loading and animated rendering
* Very draft editor features: object selection, placing
* Cross-platform (not tested yet :))
* Export Vids to CSV table and maps as a Json

# Dependencies
* sokol-gfx
* vcpkg:
	* Dear IMGUI
	* argparse
	* nlohmann-json
	* glm
    * flecs