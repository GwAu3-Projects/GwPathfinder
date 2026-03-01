#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace Pathfinder {

    /**
     * @brief Singleton registry for loading map data
     *
     * This class loads JSON map data on demand from a RAR archive
     * or directly from a maps/ directory (fallback).
     *
     * Configuration:
     * 1. Place maps.rar (or a maps/ folder) in the same directory as the DLL
     * 2. Files must be named {mapId}_*.json (e.g., 7_Prophecies_...json)
     */
    class MapDataRegistry {
    public:
        // Singleton
        static MapDataRegistry& GetInstance();

        /**
         * @brief Initializes the registry with the archive path
         * @param archive_path Path to maps.rar (optional, defaults to the DLL folder)
         * @return true if initialization succeeded (tries maps.rar, then maps/ directory)
         */
        bool Initialize(const std::string& archive_path = "");

        /**
         * @brief Returns JSON data for a map
         * @param map_id ID of the map to load
         * @return JSON data, or "" if map doesn't exist
         */
        std::string GetMapData(int32_t map_id);

        /**
         * @brief Checks if a map is available
         * @param map_id ID of the map to check
         * @return true if map exists
         */
        bool HasMap(int32_t map_id) const;

        /**
         * @brief Gets the list of all available map IDs
         * @return Vector containing all map IDs
         */
        std::vector<int32_t> GetAvailableMapIds() const;

        /**
         * @brief Checks if the registry is initialized
         */
        bool IsInitialized() const;

        // Disallow copying
        MapDataRegistry(const MapDataRegistry&) = delete;
        MapDataRegistry& operator=(const MapDataRegistry&) = delete;

    private:
        MapDataRegistry();
        ~MapDataRegistry() = default;

        /**
         * @brief Gets the DLL directory path
         * @return Path to the directory containing the DLL
         */
        std::string GetModuleDirectory() const;
    };

} // namespace Pathfinder
