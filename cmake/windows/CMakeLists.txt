cmake_minimum_required(VERSION 3.29)
project(gdsolver VERSION 0.1.0 LANGUAGES CXX)

set(GEODE_GD_VERSION 2.2081 CACHE STRING "Target game version" FORCE)
set(GEODE_COMP_GD_VERSION 22081 CACHE STRING "Target game version number" FORCE)
set(GEODE_DONT_INSTALL_MODS ON CACHE BOOL "Never install the local fork" FORCE)
set(GEODE_DISABLE_CLI_CALLS ON CACHE BOOL "Do not rewrite the global CLI profile" FORCE)
set(GEODE_DISABLE_PRECOMPILED_HEADERS ON CACHE BOOL "Match the baseline toolchain" FORCE)
set(SKIP_BUILDING_CODEGEN OFF CACHE BOOL "Use pinned Codegen sources" FORCE)
set(GDSOLVER_BUILD_MOD ON CACHE BOOL "Build the mod" FORCE)
set(GDSOLVER_TEST_INSTRUMENTS OFF CACHE BOOL "Keep upstream release defaults" FORCE)

get_filename_component(PATHWEAVER_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(GEODE_BINDINGS_PATH "${CMAKE_CURRENT_BINARY_DIR}/compat-bindings-22081" CACHE PATH "Build-only alias compatibility input" FORCE)
add_subdirectory("${PATHWEAVER_SOURCE}" fork)

add_library(pathweaver_g1_binding_abi_check OBJECT "${CMAKE_CURRENT_BINARY_DIR}/abi-check/check.cpp")
target_link_libraries(pathweaver_g1_binding_abi_check PRIVATE GeodeBindings)
target_compile_features(pathweaver_g1_binding_abi_check PRIVATE cxx_std_23)

# setup_geode_mod skips these properties when CLI calls are disabled.
target_compile_definitions(gdsolver PRIVATE GEODE_MOD_ID="b1nt1n.pathweaver")
set_target_properties(gdsolver PROPERTIES PREFIX "" OUTPUT_NAME "b1nt1n.pathweaver")
if(TARGET geode-loader)
    message(FATAL_ERROR "The local fork must not build the Geode loader")
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/package")
foreach(asset mod.json about.md changelog.md logo.png LICENSE CREDITS.md THIRD_PARTY_NOTICES.md)
    configure_file("${PATHWEAVER_SOURCE}/${asset}" "${CMAKE_CURRENT_BINARY_DIR}/package/${asset}" COPYONLY)
endforeach()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/package/resources")
configure_file("${PATHWEAVER_SOURCE}/resources/pathweaver-pause.png"
    "${CMAKE_CURRENT_BINARY_DIR}/package/resources/pathweaver-pause.png" COPYONLY)
file(COPY "${PATHWEAVER_SOURCE}/third_party/eclipse_api"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/package/third_party")
