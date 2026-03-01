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
        float z;            // Altitude (cosmetic only, pathfinding is 2D)
        int32_t layer;      // Layer/plane (0 = ground level, 1+ = elevated/bridge)
        float clearance;    // Distance to nearest wall (ray-marched)
        float score;        // Score value

        Point() : id(-1), pos(), z(0), layer(0), clearance(0), score(0) {}
        Point(int32_t _id, float _x, float _y, float _z, int32_t _layer, float _clearance = 0, float _score = 0)
            : id(_id), pos(_x, _y), z(_z), layer(_layer), clearance(_clearance), score(_score) {}
        Point(int32_t _id, const Vec2f& _pos, int32_t _layer = 0)
            : id(_id), pos(_pos), z(0), layer(_layer), clearance(0), score(0) {}
    };

    // Structure for a path point with layer (used in path results)
    struct PathPointWithLayer {
        Vec2f pos;
        int32_t layer;
        int32_t tp_type; // 0 = normal, 1 = one-way TP, 2 = two-way TP, 3 = switch TP

        PathPointWithLayer() : pos(), layer(0), tp_type(0) {}
        PathPointWithLayer(const Vec2f& _pos, int32_t _layer) : pos(_pos), layer(_layer), tp_type(0) {}
        PathPointWithLayer(float _x, float _y, int32_t _layer) : pos(_x, _y), layer(_layer), tp_type(0) {}
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
        Vec2f enter;        // Entry point
        float enter_z;      // Entry altitude
        int32_t enter_layer;// Entry layer
        Vec2f exit;         // Exit point
        float exit_z;       // Exit altitude
        int32_t exit_layer; // Exit layer
        int32_t direction;  // 0 = one-way, 1 = both-ways, 2 = both-ways (switch-activated)

        Teleporter() : enter(), enter_z(0), enter_layer(0), exit(), exit_z(0), exit_layer(0), direction(0) {}
        Teleporter(float ex, float ey, float ez, int32_t el, float xx, float xy, float xz, int32_t xl, int32_t dir)
            : enter(ex, ey), enter_z(ez), enter_layer(el), exit(xx, xy), exit_z(xz), exit_layer(xl), direction(dir) {}
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
        float z;                                    // Portal altitude
        std::vector<PortalConnection> connections;  // List of possible destinations

        TravelPortal() : position(), z(0) {}
        TravelPortal(float x, float y, float _z = 0) : position(x, y), z(_z) {}
    };

    // Structure for NPC travel
    struct NpcTravel {
        Vec2f npc_pos;          // NPC position
        float npc_z;            // NPC altitude
        int32_t npc_layer;      // NPC layer
        int32_t dialog_ids[5];  // Dialog IDs
        int32_t dest_map_id;    // Destination map ID
        Vec2f dest_pos;         // Destination position
        float dest_z;           // Destination altitude
        int32_t dest_layer;     // Destination layer

        NpcTravel() : npc_pos(), npc_z(0), npc_layer(0), dest_map_id(0), dest_pos(), dest_z(0), dest_layer(0) {
            for (int i = 0; i < 5; ++i) dialog_ids[i] = 0;
        }
        NpcTravel(float npc_x, float npc_y, float _npc_z, int32_t _npc_layer,
                  int32_t d1, int32_t d2, int32_t d3, int32_t d4, int32_t d5,
                  int32_t map_id, float dest_x, float dest_y, float _dest_z, int32_t _dest_layer)
            : npc_pos(npc_x, npc_y), npc_z(_npc_z), npc_layer(_npc_layer),
              dest_map_id(map_id), dest_pos(dest_x, dest_y), dest_z(_dest_z), dest_layer(_dest_layer) {
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
        float enter_z;          // Entry altitude
        int32_t enter_layer;    // Entry layer
        int32_t dest_map_id;    // Destination map ID
        Vec2f dest_pos;         // Destination position
        float dest_z;           // Destination altitude
        int32_t dest_layer;     // Destination layer

        EnterTravel() : enter_pos(), enter_z(0), enter_layer(0), dest_map_id(0), dest_pos(), dest_z(0), dest_layer(0) {}
        EnterTravel(float enter_x, float enter_y, float _enter_z, int32_t _enter_layer,
                    int32_t map_id, float dest_x, float dest_y, float _dest_z, int32_t _dest_layer)
            : enter_pos(enter_x, enter_y), enter_z(_enter_z), enter_layer(_enter_layer),
              dest_map_id(map_id), dest_pos(dest_x, dest_y), dest_z(_dest_z), dest_layer(_dest_layer) {}
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
            int32_t dest_layer,
            const std::vector<ObstacleZone>& obstacles,
            float& out_cost
        );

        // Simplifies a path (removes intermediate points that are too close)
        std::vector<PathPointWithLayer> SimplifyPath(
            const std::vector<PathPointWithLayer>& path,
            float min_spacing
        );

        // Sets the clearance weight for A* (0 = disabled)
        void SetClearanceWeight(float weight) { m_clearance_weight = weight; }

        // Checks if a map is loaded
        bool IsMapLoaded(int32_t map_id) const;

        // Returns the IDs of loaded maps
        std::vector<int32_t> GetLoadedMapIds() const;

        // Gets the statistics of a map
        bool GetMapStatistics(int32_t map_id, MapStatistics& out_stats) const;

        // Debug info for failed pathfinding
        struct DebugInfo {
            int point_count = 0;
            int visgraph_size = 0;
            int trap_count = 0;
            bool start_in_trap = false;
            bool goal_in_trap = false;
        };
        DebugInfo GetMapDebugInfo(int32_t map_id, const Vec2f& start, const Vec2f& goal) const;

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
        // max_search_index: only search points with index < this value (-1 = all points)
        //   Used to avoid connecting temp points to other temp points (matching JS behavior)
        void InsertPointIntoVisGraph(
            MapData& map_data,
            int32_t point_id,
            int32_t max_connections = 8,
            float max_range = 5000.0f,
            bool allow_cross_layer = false,
            int32_t max_search_index = -1
        );

        // Removes temporary points from the map data (cleanup after pathfinding)
        void RemoveTemporaryPoints(
            MapData& map_data,
            size_t original_point_count,
            size_t original_visgraph_size
        );

        // Check if a position is a teleport entrance or exit
        bool IsTeleportPoint(const MapData& map_data, const Vec2f& pos) const;

        // Tag path points that are teleporter entrances with their tp_type
        void TagTeleporterPoints(const MapData& map_data, std::vector<PathPointWithLayer>& path) const;

        // Loaded maps (map_id -> MapData)
        std::unordered_map<int32_t, MapData> m_loaded_maps;

        // Clearance weight for A* cost (0 = disabled)
        float m_clearance_weight = 0.0f;
    };

} // namespace Pathfinder
