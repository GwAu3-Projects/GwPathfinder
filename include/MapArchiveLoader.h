#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <list>
#include <cstdint>

namespace Pathfinder {

    /**
     * @brief LRU cache manager for map data
     *
     * Caches JSON data from recently used maps to avoid
     * re-reading the RAR archive each time.
     */
    class MapCache {
    public:
        explicit MapCache(size_t max_size = 20);

        // Gets map data from cache (returns "" if not found)
        std::string Get(int32_t map_id);

        // Adds or updates an entry in the cache
        void Put(int32_t map_id, const std::string& data);

        // Clears the entire cache
        void Clear();

        // Returns the current cache size
        size_t Size() const { return m_cache.size(); }

    private:
        size_t m_max_size;
        std::mutex m_mutex;

        // List to maintain LRU order (most recent = front)
        std::list<int32_t> m_lru_list;

        // Map: map_id -> (data, iterator in lru_list)
        struct CacheEntry {
            std::string data;
            std::list<int32_t>::iterator lru_it;
        };
        std::unordered_map<int32_t, CacheEntry> m_cache;
    };

    /**
     * @brief Map data loader from RAR archive or directory
     *
     * This class handles lazy loading of JSON data from a RAR archive
     * or directly from a maps/ directory. Data is loaded on demand
     * and cached to improve performance.
     */
    class MapArchiveLoader {
    public:
        MapArchiveLoader();
        ~MapArchiveLoader();

        // Singleton
        static MapArchiveLoader& GetInstance();

        /**
         * @brief Initializes the loader with the archive path
         * @param archive_path Path to the maps.rar file
         * @return true if initialization succeeded
         */
        bool Initialize(const std::string& archive_path);

        /**
         * @brief Initializes the loader with a directory path
         * @param directory_path Path to the maps/ directory
         * @return true if initialization succeeded
         */
        bool InitializeFromDirectory(const std::string& directory_path);

        /**
         * @brief Loads JSON data for a map
         * @param map_id ID of the map to load
         * @return JSON data for the map, or "" if not found
         */
        std::string LoadMapData(int32_t map_id);

        /**
         * @brief Checks if a map exists in the archive
         * @param map_id ID of the map to check
         * @return true if the map exists
         */
        bool HasMap(int32_t map_id) const;

        /**
         * @brief Gets the list of all available map IDs
         * @return Vector containing all map IDs
         */
        std::vector<int32_t> GetAvailableMapIds() const;

        /**
         * @brief Checks if the loader is initialized
         */
        bool IsInitialized() const { return m_initialized; }

        /**
         * @brief Clears the map cache
         */
        void ClearCache();

        // Disallow copying
        MapArchiveLoader(const MapArchiveLoader&) = delete;
        MapArchiveLoader& operator=(const MapArchiveLoader&) = delete;

    private:
        bool m_initialized;
        bool m_use_directory;
        std::string m_archive_path;
        std::string m_directory_path;
        mutable std::mutex m_mutex;

        // LRU cache for map data
        std::unique_ptr<MapCache> m_cache;

        // List of available map IDs in the archive
        std::vector<int32_t> m_available_maps;

        // Map ID -> filename (for directory mode)
        std::unordered_map<int32_t, std::string> m_map_filenames;

        // --- Archive mode ---
        std::string FindAndReadMapFile(int32_t map_id);
        void ScanArchive();

        // --- Directory mode ---
        std::string ReadMapFromDirectory(int32_t map_id);
        void ScanDirectory();
    };

} // namespace Pathfinder
