cmake_minimum_required(VERSION 3.25)
project(ASTROCHRON VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Quick QuickControls2 ShaderTools Network Sql)
find_program(NODE_EXECUTABLE node REQUIRED)
qt_standard_project_setup(REQUIRES 6.11)

set(map_resource "${CMAKE_CURRENT_BINARY_DIR}/generated/world-map.json")
add_custom_command(
    OUTPUT "${map_resource}"
    COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare-map.mjs" "${map_resource}"
    DEPENDS
        tools/prepare-map.mjs package-lock.json
        assets/natural-earth/source/ne_50m_land.zip
        assets/natural-earth/source/ne_50m_lakes.zip
        assets/natural-earth/source/ne_50m_admin_0_boundary_lines_land.zip
        assets/natural-earth/source/ne_50m_populated_places.zip
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM
)

add_library(astronomy_engine STATIC third_party/astronomy-engine/astronomy.c)
target_include_directories(astronomy_engine PUBLIC third_party/astronomy-engine)
if(MSVC)
    target_compile_definitions(astronomy_engine PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

qt_add_executable(astrochron WIN32 src/main.cpp)
add_library(sgp4 STATIC third_party/sgp4/SGP4.cpp)
target_include_directories(sgp4 PUBLIC third_party/sgp4)
if(MSVC)
    target_compile_definitions(sgp4 PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(sgp4 PRIVATE /utf-8)
endif()
set_target_properties(astrochron PROPERTIES OUTPUT_NAME ASTROCHRON)
qt_add_qml_module(astrochron
    URI Astrochron
    VERSION 1.0
    QML_FILES qml/Main.qml qml/SkyPlot.qml
    SOURCES
        src/app_state.h src/app_state.cpp
        src/map_data.h src/map_data.cpp
        src/world_map.h src/world_map.cpp
        src/orbit_engine.h src/orbit_engine.cpp
        src/satellite_model.h src/satellite_model.cpp
        src/weather_model.h src/weather_model.cpp
        src/orbit_layer.h src/orbit_layer.cpp
)
target_include_directories(astrochron PRIVATE src)
target_link_libraries(astrochron PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Sql astronomy_engine sgp4)
if(MSVC)
    target_compile_options(astrochron PRIVATE /utf-8 /W4)
endif()

qt_add_resources(astrochron world_map_data
    PREFIX /maps
    BASE "${CMAKE_CURRENT_BINARY_DIR}/generated"
    FILES "${map_resource}"
)
qt_add_resources(astrochron geoid_data
    PREFIX /geoid
    BASE assets/geoid
    FILES assets/geoid/egm2008-5.pgm
)
qt_add_resources(astrochron icons
    PREFIX /icons
    BASE assets/icons
    FILES
        assets/icons/plus.svg assets/icons/minus.svg assets/icons/globe.svg
        assets/icons/maximize-2.svg assets/icons/minimize-2.svg assets/icons/map-pin.svg
        assets/icons/settings-2.svg assets/icons/rotate-ccw.svg assets/icons/moon.svg
        assets/icons/refresh-cw.svg assets/icons/download.svg assets/icons/folder-open.svg
        assets/icons/copy.svg assets/icons/crosshair.svg
)
qt_add_shaders(astrochron daynight_shader
    PREFIX /
    FILES shaders/daynight.frag
)

install(TARGETS astrochron RUNTIME DESTINATION .)
install(FILES THIRD_PARTY_NOTICES.md DESTINATION .)
install(FILES third_party/astronomy-engine/LICENSE DESTINATION licenses/astronomy-engine)
install(FILES assets/icons/LICENSE DESTINATION licenses/lucide)
install(FILES third_party/sgp4/LICENSE DESTINATION licenses/sgp4)
install(FILES third_party/qt/LGPL-3.0-only.txt third_party/qt/GPL-3.0-only.txt third_party/qt/Qt-GPL-exception-1.0.txt DESTINATION licenses/qt)
