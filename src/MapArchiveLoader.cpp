#include "MapArchiveLoader.h"
#include <archive.h>
#include <archive_entry.h>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <fstream>
#include <filesystem>

namespace Pathfinder {

    // ==================== MapCache Implementation ====================

    MapCache::MapCache(size_t max_size)
        : m_max_size(max_size) {
    }

    std::string MapCache::Get(int32_t map_id) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_cache.find(map_id);
        if (it == m_cache.end()) {
            return "";
        }

        // Move this element to the front of the LRU list
        m_lru_list.erase(it->second.lru_it);
        m_lru_list.push_front(map_id);
        it->second.lru_it = m_lru_list.begin();

        return it->second.data;
    }

    void MapCache::Put(int32_t map_id, const std::string& data) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_cache.find(map_id);
        if (it != m_cache.end()) {
            // Update existing entry
            m_lru_list.erase(it->second.lru_it);
            m_lru_list.push_front(map_id);
            it->second.data = data;
            it->second.lru_it = m_lru_list.begin();
        }
        else {
            // New entry
            if (m_cache.size() >= m_max_size) {
                // Remove the least recently used element
                int32_t old_id = m_lru_list.back();
                m_lru_list.pop_back();
                m_cache.erase(old_id);
            }

            m_lru_list.push_front(map_id);
            CacheEntry entry;
            entry.data = data;
            entry.lru_it = m_lru_list.begin();
            m_cache[map_id] = entry;
        }
    }

    void MapCache::Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache.clear();
        m_lru_list.clear();
    }

    // ==================== MapArchiveLoader Implementation ====================

    MapArchiveLoader::MapArchiveLoader()
        : m_initialized(false)
        , m_use_directory(false)
        , m_cache(std::make_unique<MapCache>(20)) {
    }

    MapArchiveLoader::~MapArchiveLoader() {
    }

    MapArchiveLoader& MapArchiveLoader::GetInstance() {
        static MapArchiveLoader instance;
        return instance;
    }

    bool MapArchiveLoader::Initialize(const std::string& archive_path) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_initialized) {
            return true;
        }

        m_archive_path = archive_path;
        m_use_directory = false;

        // Verify that the archive exists and is readable
        struct archive* a = archive_read_new();
        archive_read_support_format_rar(a);
        archive_read_support_format_rar5(a);

        int r = archive_read_open_filename(a, archive_path.c_str(), 10240);
        if (r != ARCHIVE_OK) {
            archive_read_free(a);
            return false;
        }
        archive_read_free(a);

        // Scan the archive to find all available maps
        ScanArchive();

        m_initialized = true;
        return true;
    }

    bool MapArchiveLoader::InitializeFromDirectory(const std::string& directory_path) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_initialized) {
            return true;
        }

        if (!std::filesystem::is_directory(directory_path)) {
            return false;
        }

        m_directory_path = directory_path;
        m_use_directory = true;

        // Scan the directory to find all available maps
        ScanDirectory();

        m_initialized = true;
        return true;
    }

    std::string MapArchiveLoader::LoadMapData(int32_t map_id) {
        if (!m_initialized) {
            return "";
        }

        // Check the cache first
        std::string cached = m_cache->Get(map_id);
        if (!cached.empty()) {
            return cached;
        }

        // Find the file corresponding to map_id
        std::string data = m_use_directory ? ReadMapFromDirectory(map_id) : FindAndReadMapFile(map_id);
        if (!data.empty()) {
            // Add to cache
            m_cache->Put(map_id, data);
        }

        return data;
    }

    bool MapArchiveLoader::HasMap(int32_t map_id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::find(m_available_maps.begin(), m_available_maps.end(), map_id) != m_available_maps.end();
    }

    std::vector<int32_t> MapArchiveLoader::GetAvailableMapIds() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_available_maps;
    }

    void MapArchiveLoader::ClearCache() {
        m_cache->Clear();
    }

    std::string MapArchiveLoader::FindAndReadMapFile(int32_t map_id) {
        std::lock_guard<std::mutex> lock(m_mutex);

        struct archive* a = archive_read_new();
        archive_read_support_format_rar(a);
        archive_read_support_format_rar5(a);

        int r = archive_read_open_filename(a, m_archive_path.c_str(), 10240);
        if (r != ARCHIVE_OK) {
            archive_read_free(a);
            return "";
        }

        // Search for a file that starts with "map_id_"
        std::string prefix = std::to_string(map_id) + "_";
        struct archive_entry* entry;

        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            const char* name = archive_entry_pathname(entry);
            if (!name) continue;

            std::string fullpath(name);
            // Strip directory prefix (e.g. "maps/" or "maps\")
            std::string filename = fullpath;
            size_t last_sep = fullpath.find_last_of("/\\");
            if (last_sep != std::string::npos) {
                filename = fullpath.substr(last_sep + 1);
            }
            // Check if the file starts with map_id and ends with .json
            if (filename.find(prefix) == 0 && filename.find(".json") != std::string::npos) {
                // File found, read it
                int64_t size = archive_entry_size(entry);
                std::string content;

                if (size > 0) {
                    content.resize(static_cast<size_t>(size));
                    int64_t bytes_read = archive_read_data(a, &content[0], static_cast<size_t>(size));
                    if (bytes_read < 0) {
                        archive_read_free(a);
                        return "";
                    }
                    content.resize(static_cast<size_t>(bytes_read));
                } else {
                    // Size unknown, read in chunks
                    const size_t chunk_size = 65536;
                    char buffer[65536];
                    int64_t bytes_read;
                    while ((bytes_read = archive_read_data(a, buffer, chunk_size)) > 0) {
                        content.append(buffer, static_cast<size_t>(bytes_read));
                    }
                }

                archive_read_free(a);
                return content;
            }

            archive_read_data_skip(a);
        }

        archive_read_free(a);
        return "";
    }

    void MapArchiveLoader::ScanArchive() {
        m_available_maps.clear();

        struct archive* a = archive_read_new();
        archive_read_support_format_rar(a);
        archive_read_support_format_rar5(a);

        int r = archive_read_open_filename(a, m_archive_path.c_str(), 10240);
        if (r != ARCHIVE_OK) {
            archive_read_free(a);
            return;
        }

        struct archive_entry* entry;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            const char* name = archive_entry_pathname(entry);
            if (!name) continue;

            // Files are named: "maps/100_Prophecies_Kryta_...json"
            // Strip directory prefix, then extract the ID from the beginning
            std::string fullpath(name);
            std::string filename = fullpath;
            size_t last_sep = fullpath.find_last_of("/\\");
            if (last_sep != std::string::npos) {
                filename = fullpath.substr(last_sep + 1);
            }
            if (filename.find(".json") != std::string::npos) {
                // Find the first underscore
                size_t first_underscore = filename.find('_');
                if (first_underscore != std::string::npos && first_underscore > 0) {
                    std::string id_str = filename.substr(0, first_underscore);
                    try {
                        int32_t map_id = std::stoi(id_str);
                        m_available_maps.push_back(map_id);
                    }
                    catch (...) {
                        // Ignore files with invalid names
                    }
                }
            }

            archive_read_data_skip(a);
        }

        archive_read_free(a);

        // Sort IDs for easier searching
        std::sort(m_available_maps.begin(), m_available_maps.end());
    }

    // ==================== Directory Mode ====================

    void MapArchiveLoader::ScanDirectory() {
        m_available_maps.clear();
        m_map_filenames.clear();

        for (const auto& entry : std::filesystem::directory_iterator(m_directory_path)) {
            if (!entry.is_regular_file()) continue;

            std::string filename = entry.path().filename().string();
            if (filename.size() < 6 || filename.substr(filename.size() - 5) != ".json") continue;

            size_t first_underscore = filename.find('_');
            if (first_underscore == std::string::npos || first_underscore == 0) continue;

            std::string id_str = filename.substr(0, first_underscore);
            try {
                int32_t map_id = std::stoi(id_str);
                m_available_maps.push_back(map_id);
                m_map_filenames[map_id] = entry.path().string();
            }
            catch (...) {
                // Ignore files with invalid names
            }
        }

        std::sort(m_available_maps.begin(), m_available_maps.end());
    }

    std::string MapArchiveLoader::ReadMapFromDirectory(int32_t map_id) {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_map_filenames.find(map_id);
        if (it == m_map_filenames.end()) {
            return "";
        }

        std::ifstream file(it->second, std::ios::binary);
        if (!file.is_open()) {
            return "";
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

} // namespace Pathfinder
