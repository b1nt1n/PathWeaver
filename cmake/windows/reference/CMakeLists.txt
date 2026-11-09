cmake_minimum_required(VERSION 3.29)
project(PathWeaverSdkCheck LANGUAGES C CXX)

set(GEODE_GD_VERSION 2.2081 CACHE STRING "Checked game version" FORCE)
set(GEODE_COMP_GD_VERSION 22081 CACHE STRING "Checked game version number" FORCE)
set(GEODE_DONT_INSTALL_MODS ON CACHE BOOL "Never deploy from the SDK check" FORCE)
set(GEODE_DISABLE_CLI_CALLS ON CACHE BOOL "Disable mod setup and packaging" FORCE)
set(GEODE_DISABLE_PRECOMPILED_HEADERS ON CACHE BOOL "Keep the SDK check small" FORCE)
set(SKIP_BUILDING_CODEGEN OFF CACHE BOOL "Build Codegen from pinned bindings" FORCE)
add_subdirectory("$ENV{GEODE_SDK}" geode-sdk)

if(TARGET geode-loader)
    message(FATAL_ERROR "The SDK check must not build the loader")
endif()
