#pragma once

#ifdef PATHFINDER_EXPORTS
#define PATHFINDER_API __declspec(dllexport)
#else
#define PATHFINDER_API __declspec(dllimport)
#endif

#include <cstdint>

extern "C" {
    // Structure for a path point
    struct PathPoint {
        float x;
        float y;
        int32_t layer;   // Layer/plane (0 = ground level, 1+ = elevated/bridge)
        int32_t tp_type; // 0 = normal, 1 = one-way TP, 2 = two-way TP, 3 = switch TP
    };

    // Structure for the pathfinding result
    struct PathResult {
        PathPoint* points;      // Array of path points
        int32_t point_count;    // Number of points
        float total_cost;       // Total path cost
        int32_t error_code;     // 0 = success, other = error
        char error_message[256]; // Error message if applicable
    };

    // Structure for map statistics
    struct MapStats {
        int32_t trapezoid_count;    // Number of trapezoids
        int32_t point_count;        // Number of points
        int32_t teleport_count;     // Number of teleporters
        int32_t travel_portal_count; // Number of travel portals
        int32_t npc_travel_count;   // Number of NPC travels
        int32_t enter_travel_count; // Number of Enter key travels
        int32_t error_code;         // 0 = success, other = error
        char error_message[256];    // Error message if applicable
    };

    // Structure for an obstacle zone (circular area to avoid)
    struct ObstacleZone {
        float x;        // Center X coordinate
        float y;        // Center Y coordinate
        float radius;   // Radius of the obstacle zone
    };

    /**
     * @brief Finds a path between two points on a map, avoiding obstacle zones
     *
     * Points that fall within any obstacle zone will be excluded from the pathfinding graph.
     * Adjacent points outside the obstacle zones will be kept, allowing for path detours.
     *
     * @param map_id GW map ID
     * @param start_x Starting X coordinate
     * @param start_y Starting Y coordinate
     * @param start_layer Layer of the starting point (-1 = auto-detect)
     * @param dest_x Destination X coordinate
     * @param dest_y Destination Y coordinate
     * @param dest_layer Layer of the destination point (-1 = auto-detect)
     * @param obstacles Array of obstacle zones to avoid (can be NULL if obstacle_count is 0)
     * @param obstacle_count Number of obstacles in the array
     * @param range Minimum distance between simplified points (0 = no simplification)
     * @param clearance_weight Clearance weight for A* cost (0 = disabled, higher = prefer wider paths)
     * @return PathResult* Pointer to the result (must be freed with FreePathResult)
     */
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
    );

    /**
     * @brief Frees the memory allocated for a pathfinding result
     *
     * @param result Pointer to the result to free
     */
    PATHFINDER_API void FreePathResult(PathResult* result);

    /**
     * @brief Checks if a map is available in the DLL
     *
     * @param map_id GW map ID
     * @return int32_t 1 if available, 0 otherwise
     */
    PATHFINDER_API int32_t IsMapAvailable(int32_t map_id);

    /**
     * @brief Returns the list of available map IDs
     *
     * @param count Pointer to receive the number of maps
     * @return int32_t* Array of IDs (must be freed with FreeMapList)
     */
    PATHFINDER_API int32_t* GetAvailableMaps(int32_t* count);

    /**
     * @brief Frees the memory allocated for the map list
     *
     * @param map_list Pointer to the list to free
     */
    PATHFINDER_API void FreeMapList(int32_t* map_list);

    /**
     * @brief Returns the DLL version
     *
     * @return const char* Version string (do not free)
     */
    PATHFINDER_API const char* GetPathfinderVersion();

    /**
     * @brief Initializes the DLL (optional, called automatically)
     *
     * @return int32_t 1 if success, 0 otherwise
     */
    PATHFINDER_API int32_t Initialize();

    /**
     * @brief Cleans up the DLL resources
     */
    PATHFINDER_API void Shutdown();

    /**
     * @brief Loads a map from an external JSON file
     *
     * @param map_id ID of the map to load
     * @param file_path Path to the JSON file
     * @return int32_t 1 if success, 0 otherwise
     */
    PATHFINDER_API int32_t LoadMapFromFile(int32_t map_id, const char* file_path);

    /**
     * @brief Loads a map from a JSON string in memory
     *
     * @param map_id ID of the map to load
     * @param json_data Pointer to the JSON data string
     * @param json_length Length of the JSON data in bytes
     * @return int32_t 1 if success, 0 otherwise
     */
    PATHFINDER_API int32_t LoadMapFromJsonData(int32_t map_id, const char* json_data, int32_t json_length);

    /**
     * @brief Gets the statistics of a map
     *
     * @param map_id ID of the map
     * @return MapStats* Pointer to the stats (must be freed with FreeMapStats)
     */
    PATHFINDER_API MapStats* GetMapStats(int32_t map_id);

    /**
     * @brief Frees the memory allocated for the statistics
     *
     * @param stats Pointer to the stats to free
     */
    PATHFINDER_API void FreeMapStats(MapStats* stats);

}
