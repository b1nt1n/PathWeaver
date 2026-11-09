cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(gdsolver VERSION 0.1.0 LANGUAGES CXX)

# The Geode mod links the reachability solver from dp/.
option(GDSOLVER_BUILD_MOD "Build the Geode mod (requires GEODE_SDK)" ON)
# The mod's test instruments (cfg netcatch_testonly, nettrace_testonly, deathtest_testonly): ways to drop a player's requests, to
# trace them, and to flag a level for the death-data upload, which exist to measure the record gate and which a release must not
# have. OFF by default, so a normal build ignores those keys; the measurement builds turn it ON.
option(GDSOLVER_TEST_INSTRUMENTS "Build the mod's test-only instruments (never for a release)" OFF)
# The exe guard (src/mod/exe_guard.hpp): on a Geometry Dash executable the mod was not built for, none of its hooks is enabled and its start-up
# code does not run. Empty is the default -- on for the 2.209 builds (GEODE_COMP_GD_VERSION >= 22090), off for the others; ON / OFF force it.
# OFF is the way out once Geode names the 2.209 executables itself.
set(GDSOLVER_EXE_GUARD "" CACHE STRING "Exe guard: empty = default (on for 2.209 builds), ON or OFF to force")

# The search is throughput-bound, and RelWithDebInfo differed from Release here only in how it
# links: incrementally, which routes calls through jump thunks and keeps /OPT:REF and /OPT:ICF off.
# Link it the Release way (debug info kept). The code generation is unchanged, so the output is
# too; measured on leveldp, the same objects linked both ways used 7% less CPU over three
# official levels (421 -> 391 s, single thread) with byte-identical plans and traces.
if (MSVC)
    foreach (kind EXE SHARED)
        string(REGEX REPLACE "/INCREMENTAL[^ ]*" "" CMAKE_${kind}_LINKER_FLAGS_RELWITHDEBINFO
               "${CMAKE_${kind}_LINKER_FLAGS_RELWITHDEBINFO}")
        string(APPEND CMAKE_${kind}_LINKER_FLAGS_RELWITHDEBINFO " /INCREMENTAL:NO /OPT:REF /OPT:ICF")
    endforeach()
endif()

add_subdirectory(dp)

if (GDSOLVER_BUILD_MOD)
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
    # The second copy of the core is not a mod source: it is compiled as the CLI's code is,
    # below, without the mod's options and prelude.
    list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/mod/dp_bridge2.cpp)
    add_library(${PROJECT_NAME} SHARED ${SOURCES})
    # src/mod/*.hpp include each other and the solver/ headers by path from src/
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if (NOT PATHWEAVER_ECLIPSE_API)
        set(PATHWEAVER_ECLIPSE_API "${CMAKE_CURRENT_SOURCE_DIR}/third_party/eclipse_api/include")
    endif()
    if (NOT EXISTS "${PATHWEAVER_ECLIPSE_API}/eclipse/config.hpp")
        message(FATAL_ERROR "PATHWEAVER_ECLIPSE_API must contain the Eclipse 1.9.4 API headers")
    endif()
    target_include_directories(${PROJECT_NAME} PRIVATE "${PATHWEAVER_ECLIPSE_API}")
    if (GDSOLVER_TEST_INSTRUMENTS)
        target_compile_definitions(${PROJECT_NAME} PRIVATE GDSOLVER_TEST_INSTRUMENTS=1)
    endif()
    if (NOT GDSOLVER_EXE_GUARD STREQUAL "")
        if (GDSOLVER_EXE_GUARD)
            target_compile_definitions(${PROJECT_NAME} PRIVATE GDSOLVER_EXE_GUARD=1)
        else()
            target_compile_definitions(${PROJECT_NAME} PRIVATE GDSOLVER_EXE_GUARD=0)
        endif()
    endif()

    # Compile the second solver under a separate namespace with the same options.
    add_library(gdsolver_dpcore2 STATIC src/mod/dp_bridge2.cpp)
    target_include_directories(gdsolver_dpcore2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(gdsolver_dpcore2 PRIVATE gdsolver_dp)
    set_target_properties(gdsolver_dpcore2 PROPERTIES POSITION_INDEPENDENT_CODE ON)
    gdsolver_dp_options(gdsolver_dpcore2)

    # Plain signature on purpose: Geode's setup_geode_mod links without keywords, and CMake
    # refuses to mix the two forms on one target.
    target_link_libraries(${PROJECT_NAME} gdsolver_dpcore gdsolver_dpcore2)

    if (NOT DEFINED ENV{GEODE_SDK})
        message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode (or configure with -DGDSOLVER_BUILD_MOD=OFF)")
    else()
        message(STATUS "Found Geode: $ENV{GEODE_SDK}")
    endif()

    add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

    # Pin the binary replay codec; do not track the moving gdr2 branch.
    CPMAddPackage("gh:maxnut/GDReplayFormat#d8d2f0b1193a46cdf47b413dc832616dd052b6a9")
    target_link_libraries(${PROJECT_NAME} libGDR)

    setup_geode_mod(${PROJECT_NAME})
endif()
