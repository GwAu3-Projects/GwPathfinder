#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <cmath>

namespace Pathfinder {

    // Structure for a 2D point
    struct Vec2f {
        float x;
        float y;

        Vec2f() : x(0), y(0) {}
        Vec2f(float _x, float _y) : x(_x), y(_y) {}

        Vec2f operator+(const Vec2f& other) const {
            return Vec2f(x + other.x, y + other.y);
        }

        Vec2f operator-(const Vec2f& other) const {
            return Vec2f(x - other.x, y - other.y);
        }

        Vec2f operator*(float scalar) const {
            return Vec2f(x * scalar, y * scalar);
        }

        float SquaredDistance(const Vec2f& other) const {
            float dx = x - other.x;
            float dy = y - other.y;
            return dx * dx + dy * dy;
        }

        float Distance(const Vec2f& other) const {
            return std::sqrt(SquaredDistance(other));
        }
    };

    // Structure for a graph point
    struct Point {
        int32_t id;
        Vec2f pos;
        int32_t layer;  // Layer/plane (0 = ground level, 1+ = elevated/bridge)

        Point() : id(-1), pos(), layer(0) {}
        Point(int32_t _id, float _x, float _y, int32_t _layer = 0) : id(_id), pos(_x, _y), layer(_layer) {}
        Point(int32_t _id, const Vec2f& _pos, int32_t _layer = 0) : id(_id), pos(_pos), layer(_layer) {}
    };

    // Structure for a path point with layer (used in path results)
    struct PathPointWithLayer {
        Vec2f pos;
        int32_t layer;

        PathPointWithLayer() : pos(), layer(0) {}
        PathPointWithLayer(const Vec2f& _pos, int32_t _layer) : pos(_pos), layer(_layer) {}
        PathPointWithLayer(float _x, float _y, int32_t _layer) : pos(_x, _y), layer(_layer) {}
    };

    // Structure for a visibility graph edge
    struct VisibilityEdge {
        int32_t target_id;      // Target point ID
        float distance;         // Distance to target point
        std::vector<uint32_t> blocking_layers; // Layers that block this path

        VisibilityEdge() : target_id(-1), distance(0.0f) {}
        VisibilityEdge(int32_t id, float dist) : target_id(id), distance(dist) {}
        VisibilityEdge(int32_t id, float dist, const std::vector<uint32_t>& layers)
            : target_id(id), distance(dist), blocking_layers(layers) {}
    };

    // Structure for a teleporter
    struct Teleporter {
        Vec2f enter;    // Entry point
        Vec2f exit;     // Exit point
        int32_t direction; // 0 = one-way, 1 = both-ways

        Teleporter() : enter(), exit(), direction(0) {}
        Teleporter(float enter_x, float enter_y, float exit_x, float exit_y, int32_t dir)
            : enter(enter_x, enter_y), exit(exit_x, exit_y), direction(dir) {}
    };

    // Structure for a travel portal connection
    struct PortalConnection {
        int32_t dest_map_id;    // Destination map ID
        Vec2f dest_pos;         // Destination position on target map

        PortalConnection() : dest_map_id(0), dest_pos() {}
        PortalConnection(int32_t map_id, float x, float y)
            : dest_map_id(map_id), dest_pos(x, y) {}
    };

    // Structure for a travel portal
    struct TravelPortal {
        Vec2f position;                             // Portal position
        std::vector<PortalConnection> connections;  // List of possible destinations

        TravelPortal() : position() {}
        TravelPortal(float x, float y) : position(x, y) {}
    };

    // Structure for NPC travel
    struct NpcTravel {
        Vec2f npc_pos;          // NPC position
        int32_t dialog_ids[5];  // Dialog IDs
        int32_t dest_map_id;    // Destination map ID
        Vec2f dest_pos;         // Destination position

        NpcTravel() : npc_pos(), dest_map_id(0), dest_pos() {
            for (int i = 0; i < 5; ++i) dialog_ids[i] = 0;
        }
        NpcTravel(float npc_x, float npc_y, int32_t d1, int32_t d2, int32_t d3, int32_t d4, int32_t d5,
                  int32_t map_id, float dest_x, float dest_y)
            : npc_pos(npc_x, npc_y), dest_map_id(map_id), dest_pos(dest_x, dest_y) {
            dialog_ids[0] = d1;
            dialog_ids[1] = d2;
            dialog_ids[2] = d3;
            dialog_ids[3] = d4;
            dialog_ids[4] = d5;
        }
    };

    // Structure for Enter key travel
    struct EnterTravel {
        Vec2f enter_pos;        // Entry point position on the map
        int32_t dest_map_id;    // Destination map ID
        Vec2f dest_pos;         // Destination position

        EnterTravel() : enter_pos(), dest_map_id(0), dest_pos() {}
        EnterTravel(float enter_x, float enter_y, int32_t map_id, float dest_x, float dest_y)
            : enter_pos(enter_x, enter_y), dest_map_id(map_id), dest_pos(dest_x, dest_y) {}
    };

    // Structure for map statistics
    struct MapStatistics {
        int32_t trapezoid_count;
        int32_t point_count;
        int32_t teleport_count;
        int32_t travel_portal_count;
        int32_t npc_travel_count;
        int32_t enter_travel_count;

        MapStatistics() : trapezoid_count(0), point_count(0), teleport_count(0), travel_portal_count(0),
                          npc_travel_count(0), enter_travel_count(0) {}
    };

    // Structure for an obstacle zone (circular area to avoid during pathfinding)
    struct ObstacleZone {
        Vec2f center;
        float radius;
        float radius_squared; // Precomputed for faster distance checks

        ObstacleZone() : center(), radius(0), radius_squared(0) {}
        ObstacleZone(float x, float y, float r) : center(x, y), radius(r), radius_squared(r * r) {}

        // Check if a point is inside this obstacle zone
        bool Contains(const Vec2f& point) const {
            return center.SquaredDistance(point) <= radius_squared;
        }
    };

    // Structure for a trapezoid (walkable area)
    // Format: [id, layer, ax, ay, bx, by, cx, cy, dx, dy]
    // Vertices are in order: A (top-left), B (bottom-left), C (bottom-right), D (top-right)
    struct Trapezoid {
        int32_t id;
        int32_t layer;
        Vec2f a, b, c, d;  // Four vertices

        Trapezoid() : id(-1), layer(0) {}
        Trapezoid(int32_t _id, int32_t _layer, float ax, float ay, float bx, float by,
                  float cx, float cy, float dx, float dy)
            : id(_id), layer(_layer), a(ax, ay), b(bx, by), c(cx, cy), d(dx, dy) {}

        // Check if a point is inside this trapezoid
        bool ContainsPoint(const Vec2f& p) const {
            // Check if point is inside quadrilateral using cross product signs
            auto sign = [](const Vec2f& p1, const Vec2f& p2, const Vec2f& p3) {
                return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
            };

            float d1 = sign(p, a, b);
            float d2 = sign(p, b, c);
            float d3 = sign(p, c, d);
            float d4 = sign(p, d, a);

            bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0) || (d4 < 0);
            bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0) || (d4 > 0);

            return !(has_neg && has_pos);
        }
    };

    // Structure for a temporary point (created dynamically for pathfinding)
    struct TempPoint {
        Vec2f pos;
        int32_t layer;
        int32_t trapezoid_id;  // ID of the trapezoid containing this point

        TempPoint() : pos(), layer(0), trapezoid_id(-1) {}
        TempPoint(const Vec2f& _pos, int32_t _layer, int32_t _trap_id)
            : pos(_pos), layer(_layer), trapezoid_id(_trap_id) {}
    };


    // Map data structure
    struct MapData {
        int32_t map_id;
        std::vector<Point> points;
        std::vector<std::vector<VisibilityEdge>> visibility_graph;
        std::vector<Trapezoid> trapezoids;  // Walkable areas
        std::vector<Teleporter> teleporters;
        std::vector<TravelPortal> travel_portals;
        std::vector<NpcTravel> npc_travels;
        std::vector<EnterTravel> enter_travels;
        MapStatistics stats;

        MapData() : map_id(-1) {}

        bool IsValid() const {
            return map_id > 0 && !points.empty() && !visibility_graph.empty();
        }

        // Find the trapezoid containing a point (returns nullptr if not found)
        const Trapezoid* FindTrapezoidContaining(const Vec2f& pos) const {
            for (const auto& trap : trapezoids) {
                if (trap.ContainsPoint(pos)) {
                    return &trap;
                }
            }
            return nullptr;
        }
    };

    // Main pathfinding class
    class PathfinderEngine {
    public:
        PathfinderEngine() = default;
        ~PathfinderEngine() = default;

        // Loads map data from JSON
        bool LoadMapData(int32_t map_id, const std::string& json_data);

        // Finds a path between two points, avoiding obstacle zones
        // start_layer: the layer of the starting point (-1 = auto-detect)
        std::vector<PathPointWithLayer> FindPathWithObstacles(
            int32_t map_id,
            const Vec2f& start,
            int32_t start_layer,
            const Vec2f& goal,
            const std::vector<ObstacleZone>& obstacles,
            float& out_cost
        );

        // Simplifies a path (removes intermediate points that are too close)
        std::vector<PathPointWithLayer> SimplifyPath(
            const std::vector<PathPointWithLayer>& path,
            float min_spacing
        );

        // Checks if a map is loaded
        bool IsMapLoaded(int32_t map_id) const;

        // Returns the IDs of loaded maps
        std::vector<int32_t> GetLoadedMapIds() const;

        // Gets the statistics of a map
        bool GetMapStatistics(int32_t map_id, MapStatistics& out_stats) const;

    private:
        // A* algorithm
        std::vector<int32_t> AStar(
            const MapData& map_data,
            int32_t start_id,
            int32_t goal_id
        );

        // A* algorithm with obstacle avoidance
        std::vector<int32_t> AStarWithObstacles(
            const MapData& map_data,
            int32_t start_id,
            int32_t goal_id,
            const std::vector<ObstacleZone>& obstacles
        );

        // Check if a point is blocked by any obstacle
        bool IsPointBlocked(
            const Vec2f& point,
            const std::vector<ObstacleZone>& obstacles
        ) const;

        // Check if an edge (segment) is blocked by any obstacle
        bool IsEdgeBlocked(
            const Vec2f& from,
            const Vec2f& to,
            const std::vector<ObstacleZone>& obstacles
        ) const;

        // Check if a segment intersects a circle
        bool SegmentIntersectsCircle(
            const Vec2f& from,
            const Vec2f& to,
            const Vec2f& center,
            float radius
        ) const;

        // Find bypass points around an obstacle, staying within walkable areas
        std::vector<Vec2f> FindBypassPoints(
            const MapData& map_data,
            const Vec2f& from,
            const Vec2f& to,
            const ObstacleZone& obstacle,
            const std::vector<ObstacleZone>& all_obstacles
        ) const;

        // Calculate available space around a point (distance to nearest obstacle or non-walkable area)
        float CalculateAvailableSpace(
            const MapData& map_data,
            const Vec2f& point,
            const std::vector<ObstacleZone>& obstacles
        ) const;

        // Check if a point is inside any walkable trapezoid
        bool IsPointWalkable(
            const MapData& map_data,
            const Vec2f& point
        ) const;

        // Finds the closest point to a position
        int32_t FindClosestPoint(
            const MapData& map_data,
            const Vec2f& pos
        );

        // Finds the closest point to a position, excluding blocked points
        int32_t FindClosestPointAvoidingObstacles(
            const MapData& map_data,
            const Vec2f& pos,
            const std::vector<ObstacleZone>& obstacles
        );

        // Calculates the heuristic for A*
        float Heuristic(
            const MapData& map_data,
            const Vec2f& from,
            const Vec2f& to
        );

        // Heuristic with teleporters
        float TeleporterHeuristic(
            const MapData& map_data,
            const Vec2f& from,
            const Vec2f& to
        );

        // Reconstructs the path from A* results
        std::vector<PathPointWithLayer> ReconstructPath(
            const MapData& map_data,
            const std::vector<int32_t>& came_from,
            int32_t start_id,
            int32_t goal_id
        );

        // Reconstructs the path from A* results, including start point
        std::vector<PathPointWithLayer> ReconstructPathWithStart(
            const MapData& map_data,
            const std::vector<int32_t>& came_from,
            int32_t start_id,
            int32_t goal_id
        );

        // Parses a map's JSON
        bool ParseMapJson(const std::string& json_data, MapData& out_map_data);

        // Creates a temporary point if the position is inside a valid trapezoid
        // Returns the point ID (or -1 if not in a valid trapezoid)
        int32_t CreateTemporaryPoint(
            MapData& map_data,
            const Vec2f& pos
        );

        // Creates a temporary point at the given position regardless of trapezoid
        // Uses the layer of the closest existing point
        // Returns the point ID
        int32_t CreateTemporaryPointForced(
            MapData& map_data,
            const Vec2f& pos
        );

        // Creates a temporary point at the given position with a specific layer
        // Returns the point ID
        int32_t CreateTemporaryPointWithLayer(
            MapData& map_data,
            const Vec2f& pos,
            int32_t layer
        );

        // Inserts a temporary point into the visibility graph by connecting it to nearby points
        // If allow_cross_layer is true, connections can be made across different layers
        void InsertPointIntoVisGraph(
            MapData& map_data,
            int32_t point_id,
            int32_t max_connections = 8,
            float max_range = 5000.0f,
            bool allow_cross_layer = false
        );

        // Removes temporary points from the map data (cleanup after pathfinding)
        void RemoveTemporaryPoints(
            MapData& map_data,
            size_t original_point_count,
            size_t original_visgraph_size
        );

        // Loaded maps (map_id -> MapData)
        std::unordered_map<int32_t, MapData> m_loaded_maps;
    };

} // namespace Pathfinder
