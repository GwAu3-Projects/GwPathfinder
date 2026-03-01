#include "PathfinderAPI.h"
#include "PathfinderCore.h"
#include "MapDataRegistry.h"
#include <cstring>
#include <memory>
#include <fstream>
#include <sstream>

// Global pathfinding engine instance
static std::unique_ptr<Pathfinder::PathfinderEngine> g_engine;
static bool g_initialized = false;

extern "C" {

    PATHFINDER_API int32_t Initialize() {
        if (g_initialized) {
            return 1; // Already initialized
        }

        try {
            g_engine = std::make_unique<Pathfinder::PathfinderEngine>();

            // Try to initialize the map registry (maps.rar or maps/ directory)
            // This is not fatal if it fails — maps can still be loaded
            // individually via LoadMapFromFile / LoadMapFromJsonData
            auto& registry = Pathfinder::MapDataRegistry::GetInstance();
            registry.Initialize();

            g_initialized = true;
            return registry.IsInitialized() ? 1 : 2; // 2 = initialized but no maps source found
        }
        catch (...) {
            return 0;
        }
    }

    PATHFINDER_API void Shutdown() {
        if (g_initialized) {
            g_engine.reset();
            g_initialized = false;
        }
    }

    PATHFINDER_API PathResult* FindPathWithObstacles(
        int32_t map_id,
        float start_x,
        float start_y,
        int32_t start_layer,
        float dest_x,
        float dest_y,
        int32_t dest_layer,
        ObstacleZone* obstacles,
        int32_t obstacle_count,
        float range,
        float clearance_weight
    ) {
        // Auto-initialize if necessary
        if (!g_initialized) {
            if (!Initialize()) {
                PathResult* result = new PathResult();
                result->points = nullptr;
                result->point_count = 0;
                result->total_cost = -1.0f;
                result->error_code = -1;
                std::snprintf(result->error_message, sizeof(result->error_message), "Failed to initialize pathfinder");
                return result;
            }
        }

        PathResult* result = new PathResult();
        result->points = nullptr;
        result->point_count = 0;
        result->total_cost = -1.0f;
        result->error_code = 0;
        result->error_message[0] = '\0';

        try {
            // Check if the map is loaded, otherwise load it from the archive/directory
            if (!g_engine->IsMapLoaded(map_id)) {
                auto& registry = Pathfinder::MapDataRegistry::GetInstance();
                std::string map_data = registry.GetMapData(map_id);

                if (map_data.empty()) {
                    result->error_code = 1;
                    std::snprintf(result->error_message, 255, "Map %d not found in archive", map_id);
                    return result;
                }

                // Load the map into the engine
                if (!g_engine->LoadMapData(map_id, map_data)) {
                    result->error_code = 1;
                    std::snprintf(result->error_message, 255, "Failed to load map %d", map_id);
                    return result;
                }
            }

            // Convert API obstacles to internal format
            std::vector<Pathfinder::ObstacleZone> internal_obstacles;
            if (obstacles != nullptr && obstacle_count > 0) {
                internal_obstacles.reserve(obstacle_count);
                for (int32_t i = 0; i < obstacle_count; ++i) {
                    internal_obstacles.emplace_back(
                        obstacles[i].x,
                        obstacles[i].y,
                        obstacles[i].radius
                    );
                }
            }

            // Set clearance weight before pathfinding
            g_engine->SetClearanceWeight(clearance_weight);

            // Find the path with obstacle avoidance
            Pathfinder::Vec2f start(start_x, start_y);
            Pathfinder::Vec2f goal(dest_x, dest_y);
            float cost = 0.0f;

            // Use pathfinding with obstacle avoidance (pass start_layer/dest_layer, -1 means auto-detect)
            std::vector<Pathfinder::PathPointWithLayer> path =
                g_engine->FindPathWithObstacles(map_id, start, start_layer, goal, dest_layer, internal_obstacles, cost);

            if (path.empty()) {
                result->error_code = 2;
                // Add diagnostic info
                auto stats = g_engine->GetMapDebugInfo(map_id, start, goal);
                std::snprintf(result->error_message, 255,
                    "No path found (pts=%d, visg=%d, traps=%d, startTrap=%s, goalTrap=%s)",
                    stats.point_count, stats.visgraph_size, stats.trap_count,
                    stats.start_in_trap ? "yes" : "no",
                    stats.goal_in_trap ? "yes" : "no");
                return result;
            }

            // Simplify the path if requested
            if (range > 0.0f) {
                path = g_engine->SimplifyPath(path, range);
            }

            // Allocate and copy the points
            result->point_count = static_cast<int32_t>(path.size());
            result->points = new PathPoint[result->point_count];
            result->total_cost = cost;

            for (int32_t i = 0; i < result->point_count; ++i) {
                result->points[i].x = path[i].pos.x;
                result->points[i].y = path[i].pos.y;
                result->points[i].layer = path[i].layer;
                result->points[i].tp_type = path[i].tp_type;
            }

            return result;
        }
        catch (const std::exception& e) {
            result->error_code = -2;
            std::snprintf(result->error_message, 255, "Exception: %s", e.what());
            return result;
        }
        catch (...) {
            result->error_code = -3;
            std::snprintf(result->error_message, sizeof(result->error_message), "Unknown exception");
            return result;
        }
    }

    PATHFINDER_API void FreePathResult(PathResult* result) {
        if (result) {
            if (result->points) {
                delete[] result->points;
            }
            delete result;
        }
    }

    PATHFINDER_API int32_t IsMapAvailable(int32_t map_id) {
        if (!g_initialized) {
            Initialize();
        }

        auto& registry = Pathfinder::MapDataRegistry::GetInstance();
        return registry.HasMap(map_id) ? 1 : 0;
    }

    PATHFINDER_API int32_t* GetAvailableMaps(int32_t* count) {
        if (!g_initialized) {
            Initialize();
        }

        if (!count) {
            return nullptr;
        }

        try {
            // Get the list of all available maps in the archive
            auto& registry = Pathfinder::MapDataRegistry::GetInstance();
            std::vector<int32_t> map_ids = registry.GetAvailableMapIds();
            *count = static_cast<int32_t>(map_ids.size());

            if (map_ids.empty()) {
                return nullptr;
            }

            int32_t* result = new int32_t[map_ids.size()];
            std::memcpy(result, map_ids.data(), map_ids.size() * sizeof(int32_t));

            return result;
        }
        catch (...) {
            *count = 0;
            return nullptr;
        }
    }

    PATHFINDER_API void FreeMapList(int32_t* map_list) {
        if (map_list) {
            delete[] map_list;
        }
    }

    PATHFINDER_API const char* GetPathfinderVersion() {
        return "GWPathfinder v1.0.0";
    }

    PATHFINDER_API int32_t LoadMapFromFile(int32_t map_id, const char* file_path) {
        // Auto-initialize if necessary
        if (!g_initialized) {
            if (!Initialize()) {
                return 0;
            }
        }

        if (!file_path) {
            return 0;
        }

        try {
            // Open the JSON file
            std::ifstream file(file_path, std::ios::in | std::ios::binary);
            if (!file.is_open()) {
                return 0;
            }

            // Read all content
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string json_data = buffer.str();

            // Load into the engine
            if (g_engine->LoadMapData(map_id, json_data)) {
                return 1;
            }

            return 0;
        }
        catch (...) {
            return 0;
        }
    }

    PATHFINDER_API int32_t LoadMapFromJsonData(int32_t map_id, const char* json_data, int32_t json_length) {
        // Auto-initialize if necessary
        if (!g_initialized) {
            if (!Initialize()) {
                return 0;
            }
        }

        if (!json_data || json_length <= 0) {
            return 0;
        }

        try {
            std::string data(json_data, static_cast<size_t>(json_length));
            if (g_engine->LoadMapData(map_id, data)) {
                return 1;
            }
            return 0;
        }
        catch (...) {
            return 0;
        }
    }

    PATHFINDER_API MapStats* GetMapStats(int32_t map_id) {
        // Auto-initialize if necessary
        if (!g_initialized) {
            Initialize();
        }

        MapStats* result = new MapStats();

        if (!g_engine) {
            result->error_code = 1;
            std::snprintf(result->error_message, sizeof(result->error_message), "Pathfinder not initialized");
            return result;
        }

        Pathfinder::MapStatistics stats;
        if (!g_engine->GetMapStatistics(map_id, stats)) {
            result->error_code = 2;
            std::snprintf(result->error_message, sizeof(result->error_message), "Map %d not loaded", map_id);
            return result;
        }

        result->trapezoid_count = stats.trapezoid_count;
        result->point_count = stats.point_count;
        result->teleport_count = stats.teleport_count;
        result->travel_portal_count = stats.travel_portal_count;
        result->npc_travel_count = stats.npc_travel_count;
        result->enter_travel_count = stats.enter_travel_count;
        result->error_code = 0;
        result->error_message[0] = '\0';

        return result;
    }

    PATHFINDER_API void FreeMapStats(MapStats* stats) {
        if (stats) {
            delete stats;
        }
    }

} // extern "C"

// DLL Entry Point
#ifdef _WIN32
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD ul_reason_for_call, LPVOID /*lpReserved*/) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        // Automatic initialization on load
        Initialize();
        break;

    case DLL_PROCESS_DETACH:
        // Automatic cleanup on unload
        Shutdown();
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}
#endif
