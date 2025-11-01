# Client/Tools Repo
This repository contains the source for the SWG Client as well as the tools that support certain aspects of development.

## Build Instructions
This repository is configured for use with Visual Studio using msbuild/solutions and is currently ONLY compatible with **Visual Studio 2013**. To setup your environment, acquire any edition of Visual Studio 2013 and clone this repository to your machine. Open Visual Studio 2013 and use `File > Open > Project/Solution` and navigate to the path where you cloned the repository then select the solution file located at `src/build/win32/swg.sln`. Please note that all SWG applications are only compatible with win32.

The project has 3 configurations for building the applications:
* **Release** which is the version intended for public dissemination and gameplay. You may recognize this as the `_r` in the client name `SwgClient_r.exe`. 
* **Optimized** which is similar to the release client but has additional options and displays in-game for testing and is ideal for Quality Assurance or Support related activities. For example, this configuration allows for additional options like targeting static world objects, printing object information in the user interface, and releasing the camera from player attachment for custom views.
* **Debug** which is a development client that has extra features for testing and extensive logging and reporting. This build isn't particularly useful for any present application.

At present, only the `Release` version of the projects will build, but we're working on cleaning up the remainder of the configurations. As a temporary solution to accessing the features of the optimized version, you can set the constant in `production.h` to `0` regardless of the configuration.

### Tool guides

- [TreeFileRspBuilder user guide](docs/TreeFileRspBuilder.md) – building the utility, configuring search paths, and generating the `.rsp` response files.

## Modern Toolchain Preview

The modernization initiative introduces a parallel build and automation stack that can be used today without disrupting the
legacy Visual Studio workflow:

* **CMake project** – Configure with `cmake -S . -B build` to compile the shared plugin API and example plugin.
* **Plugin API** – See `src/engine/shared/plugin/PluginAPI.h` for the stable ABI that future God Client extensions will consume.
* **Example plugin** – `plugin/examples/world_builder_procedural` contains a loadable module illustrating lifecycle integration.
* **Automation CLI** – Run `python -m swg_tool --help` from `tools/swg-tool` for manifest validation, navmesh generation, and publish workflows.

These components provide a concrete foundation for further modernization work while keeping the classic client operational.

### Terrain Editor modernization

The legacy Terrain Editor that ships inside `src/engine/client/application/TerrainEditor`
needs dedicated attention before it can provide a smooth authoring experience
for new or returning world builders.  The staged roadmap for stabilising the
existing toolset, upgrading the user interface and improving rendering
capabilities lives in `docs/TerrainEditorModernizationPlan.md`.

### Rendering Modernization

DirectX 10 migration work is tracked in `docs/graphics/directx10-integration.md`. The
document outlines the steps required to stand up a parallel D3D10 renderer while keeping
the proven D3D9 path available during the transition.

