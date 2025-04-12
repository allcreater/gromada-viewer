# gromada-viewer
A small program written in C++ 23 to view some resource files from the old good game "Gromada" developed by Buka Entertainment (1999)

# Usage
Just put the binaries to the root game directory and run the program.

# Features
* Interactive view of "Vids" database (game object properties and graphics)
* Visualization of images in most of the formats used in the game
* Map loading and animated rendering
* Cross-platform (but not tested yet :))
* Export Vids to CSV table and maps as a Json

# Dependencies
* sokol-gfx
* vcpkg:
	* Dear IMGUI
	* argparse
	* nlohmann-json
	* glm