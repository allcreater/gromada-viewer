# gromada-viewer
Cross-platform asset viewer and map editor of the classic game "Gromada" developed by Buka Entertainment (1999)

# Usage
* Native: Just put the binaries to the root game directory and run the program.
* We have a [Web version! Just drop game resources to the browser :)](https://allcreater.github.io/gromada-viewer/) Only `fw.res` is required, `maps` directory is optional for map viewing

# Features
* Interactive view of "Vids" database (game object properties and graphics) with all corresponding graphics frames
* Map loading and animated rendering
* Cross-platform: tested on Windows, Linux, and even WASM
* Written in C++ 23 with modules
* Map editing: 
  * object manipulation
  * terrain editing
  * saving
* Data exporting: 
  * vid params to CSV table
  * map to JSON

# Dependencies
* [sokol-gfx](https://github.com/floooh/sokol)
* [Dear IMGUI](https://github.com/ocornut/imgui)
* [argparse](https://github.com/p-ranav/argparse)
* [nlohmann-json](https://github.com/nlohmann/json)
* [glm](https://github.com/g-truc/glm)
* [flecs](https://github.com/SanderMertens/flecs)
