#include "MapDataRegistry.h"
#include "MapArchiveLoader.h"

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#endif

#include <filesystem>

namespace Pathfinder {

    MapDataRegistry& MapDataRegistry::GetInstance() {
        static MapDataRegistry instance;
        return instance;
    }

    MapDataRegistry::MapDataRegistry() {
        // Initialization is now done via Initialize()
    }

    bool MapDataRegistry::Initialize(const std::string& archive_path) {
        // If a specific path is provided, try it directly
        if (!archive_path.empty()) {
            return MapArchiveLoader::GetInstance().Initialize(archive_path);
        }

        // Auto-detect: try maps.rar first, then maps/ directory
        // Only look in the DLL's own directory, never the working directory
        std::string dir = GetModuleDirectory();
        if (dir.empty()) {
            return false;
        }

        // Try maps.rar
        std::string rar_path = dir + "\\maps.rar";
        if (MapArchiveLoader::GetInstance().Initialize(rar_path)) {
            return true;
        }

        // Fallback: try maps/ directory
        std::string maps_dir = dir + "\\maps";
        if (MapArchiveLoader::GetInstance().InitializeFromDirectory(maps_dir)) {
            return true;
        }

        return false;
    }

    std::string MapDataRegistry::GetMapData(int32_t map_id) {
        return MapArchiveLoader::GetInstance().LoadMapData(map_id);
    }

    bool MapDataRegistry::HasMap(int32_t map_id) const {
        return MapArchiveLoader::GetInstance().HasMap(map_id);
    }

    std::vector<int32_t> MapDataRegistry::GetAvailableMapIds() const {
        return MapArchiveLoader::GetInstance().GetAvailableMapIds();
    }

    bool MapDataRegistry::IsInitialized() const {
        return MapArchiveLoader::GetInstance().IsInitialized();
    }

    std::string MapDataRegistry::GetModuleDirectory() const {
#ifdef _WIN32
        // Get the DLL module path
        char dll_path[MAX_PATH];
        HMODULE hModule = NULL;

        // Get handle to current module
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(&MapDataRegistry::GetInstance),
                               &hModule)) {
            GetModuleFileNameA(hModule, dll_path, MAX_PATH);

            // Remove filename to get just the folder
            PathRemoveFileSpecA(dll_path);
            return std::string(dll_path);
        }
#endif
        return "";
    }

} // namespace Pathfinder
