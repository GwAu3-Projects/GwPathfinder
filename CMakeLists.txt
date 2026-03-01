cmake_minimum_required(VERSION 3.16)
project(GWPathfinder VERSION 1.0.0 LANGUAGES CXX)

# Forcer le linkage statique avec vcpkg
set(VCPKG_TARGET_TRIPLET "x86-windows-static" CACHE STRING "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Configuration C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources de la DLL
set(PATHFINDER_SOURCES
    src/PathfinderAPI.cpp
    src/PathfinderCore.cpp
    src/MapDataRegistry.cpp
    src/MapArchiveLoader.cpp
)

set(PATHFINDER_HEADERS
    include/PathfinderAPI.h
    include/PathfinderCore.h
    include/MapDataRegistry.h
    include/MapArchiveLoader.h
)

# Créer la DLL
add_library(GWPathfinder SHARED
    ${PATHFINDER_SOURCES}
    ${PATHFINDER_HEADERS}
)

# Ajouter le dossier include pour les headers
target_include_directories(GWPathfinder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Définir le symbole d'export
target_compile_definitions(GWPathfinder PRIVATE PATHFINDER_EXPORTS)

# Trouver et lier les dépendances
find_package(nlohmann_json CONFIG REQUIRED)
find_package(LibArchive REQUIRED)

target_link_libraries(GWPathfinder PRIVATE
    nlohmann_json::nlohmann_json
    LibArchive::LibArchive
)

# Lier shlwapi.lib pour PathRemoveFileSpec (Windows)
if(WIN32)
    target_link_libraries(GWPathfinder PRIVATE shlwapi)
endif()

# Options de compilation
if(MSVC)
    target_compile_options(GWPathfinder PRIVATE
        /W4                # Warning level 4
        /WX-               # Don't treat warnings as errors
        /permissive-       # Standards conformance
        /MP                # Multi-processor compilation
    )

    # Optimisations pour Release
    target_compile_options(GWPathfinder PRIVATE
        $<$<CONFIG:Release>:/O2>    # Maximum optimization
        $<$<CONFIG:Release>:/Oi>    # Enable intrinsics
        $<$<CONFIG:Release>:/GL>    # Whole program optimization
    )

    # Link-time optimizations pour Release
    set_target_properties(GWPathfinder PROPERTIES
        LINK_FLAGS_RELEASE "/LTCG /OPT:REF /OPT:ICF"
    )

    # Options de linker
    target_link_options(GWPathfinder PRIVATE
        /LARGEADDRESSAWARE      # Permet d'utiliser plus de 2GB en Win32
        /INCREMENTAL:NO         # Désactive le lien incrémental
    )
else()
    target_compile_options(GWPathfinder PRIVATE
        -Wall
        -Wextra
        -pedantic
    )
endif()

# Installer la DLL
install(TARGETS GWPathfinder
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Installer les headers (pour les utilisateurs C++)
install(FILES include/PathfinderAPI.h
    DESTINATION include/GWPathfinder
)

# ========================================
# TestServer executable
# ========================================
add_executable(TestServer src/TestServer.cpp)

# TestServer needs include/ for headers (PathfinderAPI.h, httplib.h)
target_include_directories(TestServer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# JSON parsing (reuse existing dependency)
target_link_libraries(TestServer PRIVATE nlohmann_json::nlohmann_json)

# Windows: WinSock2 for httplib, shlwapi for PathRemoveFileSpec
if(WIN32)
    target_link_libraries(TestServer PRIVATE ws2_32 shlwapi)
endif()

if(MSVC)
    target_compile_options(TestServer PRIVATE
        /W4 /WX- /permissive- /MP
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/Oi>
    )
    target_link_options(TestServer PRIVATE
        /LARGEADDRESSAWARE
        /INCREMENTAL:NO
    )
endif()

# Copy Tester.html to output directory
add_custom_command(TARGET TestServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/Tester.html
        $<TARGET_FILE_DIR:TestServer>/Tester.html
    COMMENT "Copying Tester.html to output directory"
)

# Copier maps.rar dans le dossier de sortie après compilation (si présent)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/maps.rar)
    add_custom_command(TARGET GWPathfinder POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/maps.rar
            $<TARGET_FILE_DIR:GWPathfinder>/maps.rar
        COMMENT "Copying maps.rar to output directory"
    )
endif()

# Générer un fichier .def pour les exports (optionnel)
if(MSVC)
    set_target_properties(GWPathfinder PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS OFF  # Utiliser __declspec(dllexport)
    )
endif()

# Information de build
message(STATUS "========================================")
message(STATUS "GWPathfinder Configuration")
message(STATUS "========================================")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Sources: src/")
message(STATUS "  Headers: include/")
message(STATUS "========================================")
