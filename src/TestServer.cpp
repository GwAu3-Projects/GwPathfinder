// ===================================================================
// GWPathfinder Test Server
// HTTP server that bridges Tester.html to GWPathfinder.dll
// Usage: TestServer.exe [--port N] [--dll path]
// ===================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>

#include "httplib.h"
#include "PathfinderAPI.h"
#include <nlohmann/json.hpp>

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <cstdint>

using json = nlohmann::json;

// ========================================
// DLL Function Pointer Typedefs
// ========================================
typedef int32_t     (*FnInitialize)();
typedef void        (*FnShutdown)();
typedef PathResult* (*FnFindPathWithObstacles)(int32_t, float, float, int32_t, float, float, int32_t, ObstacleZone*, int32_t, float, float);
typedef void        (*FnFreePathResult)(PathResult*);
typedef int32_t     (*FnIsMapAvailable)(int32_t);
typedef int32_t*    (*FnGetAvailableMaps)(int32_t*);
typedef void        (*FnFreeMapList)(int32_t*);
typedef const char* (*FnGetPathfinderVersion)();
typedef int32_t     (*FnLoadMapFromFile)(int32_t, const char*);
typedef int32_t     (*FnLoadMapFromJsonData)(int32_t, const char*, int32_t);
typedef MapStats*   (*FnGetMapStats)(int32_t);
typedef void        (*FnFreeMapStats)(MapStats*);

// ========================================
// Global State
// ========================================
static HMODULE g_dll = nullptr;
static FnInitialize             g_Initialize = nullptr;
static FnShutdown               g_Shutdown = nullptr;
static FnFindPathWithObstacles  g_FindPathWithObstacles = nullptr;
static FnFreePathResult         g_FreePathResult = nullptr;
static FnIsMapAvailable         g_IsMapAvailable = nullptr;
static FnGetAvailableMaps       g_GetAvailableMaps = nullptr;
static FnFreeMapList            g_FreeMapList = nullptr;
static FnGetPathfinderVersion   g_GetPathfinderVersion = nullptr;
static FnLoadMapFromFile        g_LoadMapFromFile = nullptr;
static FnLoadMapFromJsonData    g_LoadMapFromJsonData = nullptr;
static FnGetMapStats            g_GetMapStats = nullptr;
static FnFreeMapStats           g_FreeMapStats = nullptr;

static std::mutex g_dll_mutex;
static httplib::Server* g_server_ptr = nullptr;

// ========================================
// Utility Functions
// ========================================
static std::string GetExeDirectory() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    PathRemoveFileSpecA(path);
    return std::string(path);
}

static std::string ReadFileToString(const std::string& filepath) {
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs.is_open()) return "";
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

// ========================================
// DLL Loading
// ========================================
static bool LoadDll(const std::string& dll_path) {
    g_dll = LoadLibraryA(dll_path.c_str());
    if (!g_dll) {
        std::cerr << "  ERROR: Cannot load " << dll_path << " (error " << GetLastError() << ")" << std::endl;
        return false;
    }

    #define LOAD_FUNC(name) \
        g_##name = (Fn##name)GetProcAddress(g_dll, #name); \
        if (!g_##name) { std::cerr << "  WARNING: " #name " not found" << std::endl; }

    LOAD_FUNC(Initialize);
    LOAD_FUNC(Shutdown);
    LOAD_FUNC(FindPathWithObstacles);
    LOAD_FUNC(FreePathResult);
    LOAD_FUNC(IsMapAvailable);
    LOAD_FUNC(GetAvailableMaps);
    LOAD_FUNC(FreeMapList);
    LOAD_FUNC(GetPathfinderVersion);
    LOAD_FUNC(LoadMapFromFile);
    LOAD_FUNC(LoadMapFromJsonData);
    LOAD_FUNC(GetMapStats);
    LOAD_FUNC(FreeMapStats);

    #undef LOAD_FUNC

    // Critical functions check
    if (!g_FindPathWithObstacles || !g_FreePathResult || !g_GetPathfinderVersion) {
        std::cerr << "  ERROR: Critical DLL functions missing" << std::endl;
        FreeLibrary(g_dll);
        g_dll = nullptr;
        return false;
    }

    return true;
}

static void UnloadDll() {
    if (g_dll) {
        if (g_Shutdown) g_Shutdown();
        FreeLibrary(g_dll);
        g_dll = nullptr;
    }
}

// ========================================
// Console Ctrl Handler
// ========================================
static BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        std::cout << "\nShutting down..." << std::endl;
        if (g_server_ptr) g_server_ptr->stop();
        return TRUE;
    }
    return FALSE;
}

// ========================================
// Main
// ========================================
int main(int argc, char* argv[]) {
    // Parse arguments
    int port = 8080;
    std::string dll_name = "GWPathfinder.dll";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (arg == "--dll" && i + 1 < argc) {
            dll_name = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: TestServer.exe [--port N] [--dll path] [--help]" << std::endl;
            return 0;
        }
    }

    std::string exe_dir = GetExeDirectory();

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  GWPathfinder Test Server" << std::endl;
    std::cout << "========================================" << std::endl;

    // Load DLL
    std::string dll_path = exe_dir + "\\" + dll_name;
    std::cout << "  Loading DLL: " << dll_path << std::endl;

    if (!LoadDll(dll_path)) {
        std::cerr << "  FATAL: Cannot load DLL" << std::endl;
        return 1;
    }
    std::cout << "  DLL: loaded OK" << std::endl;

    // DLL auto-initializes in DllMain, but call Initialize as safety check
    if (g_Initialize) g_Initialize();

    const char* version = g_GetPathfinderVersion ? g_GetPathfinderVersion() : "unknown";
    std::cout << "  Version: " << version << std::endl;

    // Count available maps
    int32_t map_count = 0;
    if (g_GetAvailableMaps) {
        int32_t* maps = g_GetAvailableMaps(&map_count);
        if (maps && g_FreeMapList) g_FreeMapList(maps);
    }
    std::cout << "  Available maps: " << map_count << std::endl;
    std::cout << "  Port: " << port << std::endl;

    // Create HTTP server
    httplib::Server svr;
    g_server_ptr = &svr;

    // Allow large payloads (map JSON can be 50+ MB)
    svr.set_payload_max_length(100 * 1024 * 1024); // 100 MB

    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    // CORS headers
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    // OPTIONS preflight
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    // ========================================
    // Static file serving
    // ========================================
    svr.Get("/", [&exe_dir](const httplib::Request&, httplib::Response& res) {
        std::string html = ReadFileToString(exe_dir + "\\Tester.html");
        if (html.empty()) {
            res.status = 404;
            res.set_content("{\"error\":\"Tester.html not found\"}", "application/json");
        } else {
            res.set_content(html, "text/html");
        }
    });

    // ========================================
    // API: Version
    // ========================================
    svr.Get("/api/version", [](const httplib::Request&, httplib::Response& res) {
        json j;
        j["version"] = g_GetPathfinderVersion ? g_GetPathfinderVersion() : "unknown";
        res.set_content(j.dump(), "application/json");
    });

    // ========================================
    // API: Available Maps
    // ========================================
    svr.Get("/api/maps", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_dll_mutex);

        json j;
        j["maps"] = json::array();
        j["count"] = 0;

        if (g_GetAvailableMaps) {
            int32_t count = 0;
            int32_t* maps = g_GetAvailableMaps(&count);
            if (maps) {
                for (int32_t i = 0; i < count; i++) {
                    j["maps"].push_back(maps[i]);
                }
                j["count"] = count;
                if (g_FreeMapList) g_FreeMapList(maps);
            }
        }

        res.set_content(j.dump(), "application/json");
    });

    // ========================================
    // API: Map Stats
    // ========================================
    svr.Get(R"(/api/maps/(\d+)/stats)", [](const httplib::Request& req, httplib::Response& res) {
        int32_t map_id = std::stoi(req.matches[1]);
        std::lock_guard<std::mutex> lock(g_dll_mutex);

        json j;
        j["map_id"] = map_id;

        if (!g_GetMapStats || !g_FreeMapStats) {
            j["error_code"] = -1;
            j["error_message"] = "GetMapStats not available";
            res.set_content(j.dump(), "application/json");
            return;
        }

        MapStats* stats = g_GetMapStats(map_id);
        if (!stats) {
            j["error_code"] = -1;
            j["error_message"] = "Failed to get stats";
            res.set_content(j.dump(), "application/json");
            return;
        }

        j["error_code"] = stats->error_code;
        if (stats->error_code != 0) {
            j["error_message"] = stats->error_message;
        } else {
            j["trapezoid_count"] = stats->trapezoid_count;
            j["point_count"] = stats->point_count;
            j["teleport_count"] = stats->teleport_count;
            j["travel_portal_count"] = stats->travel_portal_count;
            j["npc_travel_count"] = stats->npc_travel_count;
            j["enter_travel_count"] = stats->enter_travel_count;
        }

        g_FreeMapStats(stats);
        res.set_content(j.dump(), "application/json");
    });

    // ========================================
    // API: Map Available check
    // ========================================
    svr.Get(R"(/api/maps/(\d+)/available)", [](const httplib::Request& req, httplib::Response& res) {
        int32_t map_id = std::stoi(req.matches[1]);
        std::lock_guard<std::mutex> lock(g_dll_mutex);

        json j;
        j["map_id"] = map_id;
        j["available"] = g_IsMapAvailable ? (g_IsMapAvailable(map_id) != 0) : false;
        res.set_content(j.dump(), "application/json");
    });

    // ========================================
    // API: Find Path
    // ========================================
    svr.Post("/api/findpath", [](const httplib::Request& req, httplib::Response& res) {
        json j_response;

        try {
            json j_req = json::parse(req.body);

            int32_t map_id = j_req.value("map_id", 0);
            float start_x = j_req.value("start_x", 0.0f);
            float start_y = j_req.value("start_y", 0.0f);
            int32_t start_layer = j_req.value("start_layer", -1);
            float dest_x = j_req.value("dest_x", 0.0f);
            float dest_y = j_req.value("dest_y", 0.0f);
            int32_t dest_layer = j_req.value("dest_layer", -1);
            float range = j_req.value("range", 0.0f);
            float clearance_weight = j_req.value("clearance_weight", 0.0f);

            // Parse obstacles
            std::vector<ObstacleZone> obstacles;
            if (j_req.contains("obstacles") && j_req["obstacles"].is_array()) {
                for (const auto& obs : j_req["obstacles"]) {
                    ObstacleZone oz;
                    oz.x = obs.value("x", 0.0f);
                    oz.y = obs.value("y", 0.0f);
                    oz.radius = obs.value("radius", 0.0f);
                    obstacles.push_back(oz);
                }
            }

            // Log request
            std::cout << "[POST /api/findpath] request: map=" << map_id
                      << " start=(" << start_x << "," << start_y << ") layer=" << start_layer
                      << " dest=(" << dest_x << "," << dest_y << ")"
                      << " range=" << range << " clearance=" << clearance_weight
                      << " obstacles=" << obstacles.size() << std::endl;

            // Call DLL
            std::lock_guard<std::mutex> lock(g_dll_mutex);

            auto t_start = std::chrono::high_resolution_clock::now();

            PathResult* result = g_FindPathWithObstacles(
                map_id, start_x, start_y, start_layer,
                dest_x, dest_y, dest_layer,
                obstacles.empty() ? nullptr : obstacles.data(),
                static_cast<int32_t>(obstacles.size()),
                range, clearance_weight
            );

            auto t_end = std::chrono::high_resolution_clock::now();
            double computation_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

            if (!result) {
                j_response["error_code"] = -1;
                j_response["error_message"] = "DLL returned null";
                j_response["total_cost"] = -1.0;
                j_response["point_count"] = 0;
                j_response["points"] = json::array();
                j_response["computation_time_ms"] = computation_ms;
                res.set_content(j_response.dump(), "application/json");
                return;
            }

            j_response["error_code"] = result->error_code;
            j_response["total_cost"] = result->total_cost;
            j_response["point_count"] = result->point_count;
            j_response["computation_time_ms"] = computation_ms;

            if (result->error_code != 0) {
                j_response["error_message"] = result->error_message;
            }

            json points_arr = json::array();
            if (result->points && result->point_count > 0) {
                for (int32_t i = 0; i < result->point_count; i++) {
                    json pt;
                    pt["x"] = result->points[i].x;
                    pt["y"] = result->points[i].y;
                    pt["layer"] = result->points[i].layer;
                    points_arr.push_back(pt);
                }
            }
            j_response["points"] = points_arr;

            // Log
            std::cout << "[POST /api/findpath] map=" << map_id
                      << " points=" << result->point_count
                      << " cost=" << result->total_cost
                      << " time=" << computation_ms << "ms";
            if (result->error_code != 0) {
                std::cout << " ERROR(" << result->error_code << "): " << result->error_message;
            }
            std::cout << std::endl;

            g_FreePathResult(result);

        } catch (const std::exception& e) {
            j_response["error_code"] = -1;
            j_response["error_message"] = std::string("Parse error: ") + e.what();
            j_response["total_cost"] = -1.0;
            j_response["point_count"] = 0;
            j_response["points"] = json::array();
        }

        res.set_content(j_response.dump(), "application/json");
    });

    // ========================================
    // API: Load Map From File
    // ========================================
    svr.Post("/api/loadmap", [](const httplib::Request& req, httplib::Response& res) {
        json j;

        try {
            json j_req = json::parse(req.body);
            int32_t map_id = j_req.value("map_id", 0);
            std::string file_path = j_req.value("file_path", "");

            if (file_path.empty() || !g_LoadMapFromFile) {
                j["success"] = false;
                j["error"] = "Invalid parameters or LoadMapFromFile not available";
                res.set_content(j.dump(), "application/json");
                return;
            }

            std::lock_guard<std::mutex> lock(g_dll_mutex);
            int32_t ok = g_LoadMapFromFile(map_id, file_path.c_str());

            j["success"] = (ok != 0);
            j["map_id"] = map_id;
            if (!ok) j["error"] = "LoadMapFromFile failed";

            std::cout << "[POST /api/loadmap] map=" << map_id
                      << " file=" << file_path
                      << " result=" << (ok ? "OK" : "FAILED") << std::endl;

        } catch (const std::exception& e) {
            j["success"] = false;
            j["error"] = std::string("Parse error: ") + e.what();
        }

        res.set_content(j.dump(), "application/json");
    });

    // ========================================
    // API: Load Map from JSON data (browser sends map data directly)
    // ========================================
    svr.Post(R"(/api/maps/(\d+)/load)", [](const httplib::Request& req, httplib::Response& res) {
        int32_t map_id = std::stoi(req.matches[1]);
        json j;

        if (!g_LoadMapFromJsonData) {
            j["success"] = false;
            j["error"] = "LoadMapFromJsonData not available in DLL";
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (req.body.empty()) {
            j["success"] = false;
            j["error"] = "Empty body";
            res.set_content(j.dump(), "application/json");
            return;
        }

        std::lock_guard<std::mutex> lock(g_dll_mutex);

        auto t_start = std::chrono::high_resolution_clock::now();
        int32_t ok = g_LoadMapFromJsonData(map_id, req.body.c_str(), static_cast<int32_t>(req.body.size()));
        auto t_end = std::chrono::high_resolution_clock::now();
        double load_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

        j["success"] = (ok != 0);
        j["map_id"] = map_id;
        j["json_size"] = req.body.size();
        j["load_time_ms"] = load_ms;
        if (!ok) j["error"] = "LoadMapFromJsonData failed";

        std::cout << "[POST /api/maps/" << map_id << "/load] size=" << req.body.size()
                  << " time=" << load_ms << "ms"
                  << " result=" << (ok ? "OK" : "FAILED") << std::endl;

        res.set_content(j.dump(), "application/json");
    });

    // ========================================
    // Start Server
    // ========================================
    std::cout << "========================================" << std::endl;
    std::cout << "  Open http://localhost:" << port << " in your browser" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Logger
    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        if (req.path.find("/api/") != std::string::npos) {
            std::cout << "[" << req.method << " " << req.path << "] -> " << res.status << std::endl;
        }
    });

    if (!svr.listen("0.0.0.0", port)) {
        std::cerr << "ERROR: Failed to start server on port " << port << std::endl;
        UnloadDll();
        return 1;
    }

    UnloadDll();
    std::cout << "Server stopped." << std::endl;
    return 0;
}
