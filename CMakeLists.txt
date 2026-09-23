cmake_minimum_required(VERSION 3.25)
project(ASTROCHRON VERSION 0.1.0 LANGUAGES C CXX)
set(ASTROCHRON_VERSION "${PROJECT_VERSION}-preview")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Quick QuickControls2 ShaderTools Network Sql LinguistTools)
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
target_compile_definitions(astrochron PRIVATE ASTROCHRON_VERSION="${ASTROCHRON_VERSION}")
add_library(sgp4 STATIC third_party/sgp4/SGP4.cpp)
target_include_directories(sgp4 PUBLIC third_party/sgp4)
if(MSVC)
    target_compile_definitions(sgp4 PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(sgp4 PRIVATE /utf-8)
endif()
set_target_properties(astrochron PROPERTIES OUTPUT_NAME ASTROCHRON)
if(WIN32)
    enable_language(RC)
    set(app_icon_rc "${CMAKE_CURRENT_BINARY_DIR}/astrochron.rc")
    configure_file(src/astrochron.rc.in "${app_icon_rc}" @ONLY)
    set_source_files_properties("${app_icon_rc}" PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/app-icon/astrochron.ico")
    target_sources(astrochron PRIVATE "${app_icon_rc}")
endif()
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(astrochron
    URI Astrochron
    VERSION 1.0
    QML_FILES
        qml/Main.qml qml/SkyPlot.qml qml/Theme.qml
        qml/IconButton.qml qml/DetailTab.qml qml/FieldRow.qml
        qml/SatelliteSidebar.qml qml/MapToolbar.qml qml/LayerMenu.qml
        qml/MapWorkspace.qml qml/PassTable.qml qml/ObservationPane.qml qml/Timeline.qml
        qml/ObserverDialog.qml qml/SettingsDialog.qml qml/LanguageSettings.qml
        qml/CatalogDialog.qml qml/PhotometryDialog.qml qml/PlanDialog.qml qml/SnapshotDialog.qml
    SOURCES
        src/app_state.h src/app_state.cpp
        src/map_data.h src/map_data.cpp
        src/world_map.h src/world_map.cpp
        src/orbit_engine.h src/orbit_engine.cpp
        src/satellite_model.h src/satellite_model.cpp
        src/weather_model.h src/weather_model.cpp
        src/catalog_model.h src/catalog_model.cpp
        src/orbit_layer.h src/orbit_layer.cpp
        src/update_checker.h src/update_checker.cpp
)
target_include_directories(astrochron PRIVATE src)
qt_add_translations(astrochron
    TS_FILES translations/astrochron_en.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete -locations none
)
target_link_libraries(astrochron PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Sql astronomy_engine sgp4)
if(MSVC)
    target_compile_options(astrochron PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/utf-8> $<$<COMPILE_LANGUAGE:CXX>:/W4>)
endif()

qt_add_resources(astrochron world_map_data
    PREFIX /maps
    BASE "${CMAKE_CURRENT_BINARY_DIR}/generated"
    FILES "${map_resource}"
)
set(geoid_resource "${CMAKE_CURRENT_BINARY_DIR}/generated/egm2008-5.bin")
add_custom_command(
    OUTPUT "${geoid_resource}"
    COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare-geoid.mjs" "${geoid_resource}"
    DEPENDS tools/prepare-geoid.mjs assets/geoid/egm2008-5.pgm
    VERBATIM
)
qt_add_resources(astrochron geoid_data
    PREFIX /geoid
    BASE "${CMAKE_CURRENT_BINARY_DIR}/generated"
    FILES "${geoid_resource}"
)
qt_add_resources(astrochron photometry_data
    PREFIX /photometry
    BASE assets/photometry
    FILES assets/photometry/qs.mag
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
        assets/icons/info.svg
        assets/icons/pin.svg
        assets/icons/x.svg assets/icons/chevron-down.svg assets/icons/chevron-right.svg
)
qt_add_resources(astrochron app_icon
    PREFIX /app
    BASE assets/app-icon
    FILES assets/app-icon/astrochron.ico
)
qt_add_shaders(astrochron daynight_shader
    PREFIX /
    FILES shaders/daynight.frag
)

install(TARGETS astrochron RUNTIME DESTINATION .)
install(FILES LICENSE THIRD_PARTY_NOTICES.md DESTINATION .)
install(FILES third_party/astronomy-engine/LICENSE DESTINATION licenses/astronomy-engine)
install(FILES assets/icons/LICENSE DESTINATION licenses/lucide)
install(FILES assets/photometry/NOTICE assets/photometry/QUICKSAT.txt DESTINATION licenses/data/photometry)
install(FILES third_party/sgp4/LICENSE DESTINATION licenses/sgp4)
install(FILES third_party/qt/LGPL-3.0-only.txt third_party/qt/GPL-3.0-only.txt third_party/qt/Qt-GPL-exception-1.0.txt DESTINATION licenses/qt)
