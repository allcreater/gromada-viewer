# gromada-viewer
Cross-platform asset viewer of the classic game "Gromada" developed by Buka Entertainment (1999)

# Usage
* Native: Just put the binaries to the root game directory and run the program.
* Web version: https://allcreater.github.io/gromada-viewer/ — just drop game resources to the browser. Only fw.res is required, maps directory is optional


# Features
* Interactive view of "Vids" database (game object properties and graphics) with all corresponding graphics frames
* Map loading and animated rendering
* Cross-platform
* C++ 23 with modules
* Rough map editor features: object manipulation and map saving
* Exporters: Vid params to CSV table, Map to JSON

# Dependencies
* sokol-gfx
* Dear IMGUI
* argparse
* nlohmann-json
* glm
* flecs
