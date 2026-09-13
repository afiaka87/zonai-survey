cmake_minimum_required(VERSION 3.25)
project(zonai_survey CXX C ASM)
option(SURVEY_DEPTH_SCAN "Use depth-based surface rendering" ON)
option(SURVEY_FIDELITY_PLAYGROUND "Enable diagnostic controls and shaders" OFF)
option(SURVEY_COMPACT_MAP "Losslessly pack collectible placements" ON)

set(EXLAUNCH_ROOT "" CACHE PATH "Path to the pinned exlaunch checkout")
set(TOTK_SDK_ROOT "" CACHE PATH "Path to compatible TotK headers and 1.2.1 symbols")
set(TOTK_VERSION 121 CACHE STRING "TotK symbol version")

if(NOT SWITCH)
  message(FATAL_ERROR "Configure with cmake/TotkToolchain.cmake")
endif()
if(NOT IS_DIRECTORY "${EXLAUNCH_ROOT}/source")
  message(FATAL_ERROR "Set EXLAUNCH_ROOT to an exlaunch checkout with the supplied patches")
endif()
if(NOT IS_DIRECTORY "${TOTK_SDK_ROOT}/syms/output/TOTK_${TOTK_VERSION}")
  message(FATAL_ERROR "Set TOTK_SDK_ROOT to compatible headers and TOTK_${TOTK_VERSION} symbols")
endif()

set(CONFIG_TITLE_ID 0x0100F2C0115B6000)
add_compile_definitions(
  TOTK_VERSION=${TOTK_VERSION}
  TOTK_${TOTK_VERSION}=1
  OVERLAY_DEBUG_HUD=0
  SOLO_HARNESS_TEXT=0
  AUDIO_DEBUG_CAPTURE=0
  PORTER_SIGIL_PROBE=0
  LOTUSKIT_HACK_SKIP_PRIVATE=1
  NNSDK=1
  NN_SDK="15.3.1"
  NN_SDK_MAJOR=15
  NN_SDK_MINOR=3
  NN_SDK_PATCH=1
  NN_SDK_TYPE="Release"
  NN_WARE="1.6.1"
  NN_WARE_MAJOR=1
  NN_WARE_MINOR=6
  NN_WARE_PATCH=1
  EXL_PROGRAM_ID=${CONFIG_TITLE_ID}
  EXL_LOAD_KIND=Module
  EXL_LOAD_KIND_ENUM=2
)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

include_directories(
  "${PROJECT_SOURCE_DIR}/src/lib"
  "${EXLAUNCH_ROOT}/source"
  "${EXLAUNCH_ROOT}/source/lib"
  "${TOTK_SDK_ROOT}/nnheaders/include"
  "${TOTK_SDK_ROOT}/agl/include"
  "${TOTK_SDK_ROOT}/sead/include"
  "${TOTK_SDK_ROOT}/syms/output/TOTK_${TOTK_VERSION}"
  "${PROJECT_SOURCE_DIR}/src"
  "${PROJECT_SOURCE_DIR}/src/program"
  "${PROJECT_SOURCE_DIR}/src/pure"
  "${PROJECT_SOURCE_DIR}/src/engine"
  "${PROJECT_SOURCE_DIR}/src/feature"
  "${PROJECT_SOURCE_DIR}/src/presentation"
  "${PROJECT_SOURCE_DIR}/src/generated"
  "${PROJECT_SOURCE_DIR}/runtime-support/include"
)

file(GLOB_RECURSE EXL_SOURCES_ASM "${EXLAUNCH_ROOT}/source/*.s")
file(GLOB_RECURSE EXL_SOURCES_H "${EXLAUNCH_ROOT}/source/*.h" "${EXLAUNCH_ROOT}/source/*.hpp")
file(GLOB_RECURSE EXL_SOURCES_CXX "${EXLAUNCH_ROOT}/source/*.cpp" "${EXLAUNCH_ROOT}/source/*.c")
list(FILTER EXL_SOURCES_CXX EXCLUDE REGEX "[/\\\\]source[/\\\\]program[/\\\\]")
file(GLOB_RECURSE SOURCES_ASM "${PROJECT_SOURCE_DIR}/src/*.s")
file(GLOB_RECURSE SOURCES_H "${PROJECT_SOURCE_DIR}/src/*.h" "${PROJECT_SOURCE_DIR}/src/*.hpp")
file(GLOB_RECURSE SOURCES_CXX CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp" "${PROJECT_SOURCE_DIR}/src/*.c")

set(SUPPORT_SOURCES
  runtime-support/src/TextWriter.cpp
  runtime-support/src/Overlay.cpp
  runtime-support/src/SoloHarness.cpp
  runtime-support/src/SdCardMount.cpp
  runtime-support/src/DepthState.cpp
  runtime-support/src/WorldDraw.cpp
)

configure_file(cmake/templates/subsdk9.json.template subsdk9.json)
configure_file(cmake/templates/subsdk9.ld subsdk9.ld COPYONLY)
set(LINKER_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/subsdk9.ld")
set(SDK_ROOT "${TOTK_SDK_ROOT}")
configure_file(subsdk9.specs.in subsdk9.specs @ONLY)

include(cmake/SwitchTools.cmake)
add_executable(subsdk9
  ${EXL_SOURCES_ASM}
  ${EXL_SOURCES_H}
  ${EXL_SOURCES_CXX}
  ${SOURCES_ASM}
  ${SOURCES_H}
  ${SOURCES_CXX}
  ${SUPPORT_SOURCES}
)
target_compile_definitions(subsdk9 PRIVATE
  SURVEY_DEPTH_SCAN=$<BOOL:${SURVEY_DEPTH_SCAN}>
  SURVEY_FIDELITY_PLAYGROUND=$<BOOL:${SURVEY_FIDELITY_PLAYGROUND}>)
get_target_property(SURVEY_SOURCES subsdk9 SOURCES)
if(SURVEY_DEPTH_SCAN)
  list(FILTER SURVEY_SOURCES EXCLUDE REGEX "/(ScanController|ScanRenderer|TerrainProber|VerticalSurfaceProber|WaterSurfaceProbe)\\.cpp$")
  include(cmake/SurfaceScan.cmake)
else()
  list(FILTER SURVEY_SOURCES EXCLUDE REGEX "/surface/")
endif()
set_property(TARGET subsdk9 PROPERTY SOURCES "${SURVEY_SOURCES}")
if(SURVEY_COMPACT_MAP)
  include(cmake/PackedGlyphs.cmake)
  survey_add_packed_glyphs(subsdk9)
  target_compile_definitions(subsdk9 PRIVATE SURVEY_COMPACT_MAP=1)
endif()

set_target_properties(subsdk9 PROPERTIES SUFFIX ".elf" C_STANDARD 23 CXX_STANDARD 26)
target_compile_options(subsdk9 PRIVATE -Werror=switch)
add_custom_command(TARGET subsdk9 POST_BUILD
  COMMAND "${CMAKE_COMMAND}" "-DNM=${CMAKE_NM}" "-DELF=$<TARGET_FILE:subsdk9>"
          -P "${PROJECT_SOURCE_DIR}/cmake/CheckImports.cmake"
  VERBATIM
  COMMENT "Checking module imports"
)
add_nso_target_subsdk(subsdk9)
set_target_properties(subsdk9 PROPERTIES CONFIG_JSON "${CMAKE_CURRENT_BINARY_DIR}/subsdk9.json")
__generate_npdm(subsdk9)
set_target_properties(subsdk9 PROPERTIES
  LINK_FLAGS "-specs ${CMAKE_BINARY_DIR}/subsdk9.specs -g ${ARCH} -nostartfiles"
)
add_custom_target(subsdk9_meta DEPENDS create_npdm subsdk9_nso)

if(SURVEY_DEPTH_SCAN)
  set(surface_budget_args)
  if(SURVEY_COMPACT_MAP)
    list(APPEND surface_budget_args --compact)
  endif()
  if(NOT SURVEY_FIDELITY_PLAYGROUND)
    list(APPEND surface_budget_args --production)
  endif()
  add_custom_target(surface_memory_check
    COMMAND uv run --no-project python "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_surface_budget.py"
      --nso "${CMAKE_CURRENT_BINARY_DIR}/subsdk9" --elf "$<TARGET_FILE:subsdk9>"
      --nm "${CMAKE_NM}" ${surface_budget_args}
    DEPENDS subsdk9_nso VERBATIM)
  add_dependencies(subsdk9_meta surface_memory_check)
endif()
