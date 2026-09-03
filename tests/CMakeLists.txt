cmake_minimum_required(VERSION 3.25)
project(zonai_survey_host_tests LANGUAGES CXX)

set(DOCTEST_DIR "" CACHE PATH "Optional path to a local doctest v2.4.11 checkout")
if(DOCTEST_DIR)
  add_library(doctest_local INTERFACE)
  target_include_directories(doctest_local INTERFACE "${DOCTEST_DIR}")
  add_library(doctest::doctest ALIAS doctest_local)
  set(DOCTEST_CMAKE "${DOCTEST_DIR}/doctest.cmake")
else()
  include(FetchContent)
  FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(doctest)
  set(DOCTEST_CMAKE "${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
endif()

add_executable(zonai_survey_pure_tests
  test_main.cpp
  test_pulse_lattice.cpp
  test_scan_batching.cpp
  test_scan_reduction.cpp
  test_scan_verdicts.cpp
  test_wall_incidence.cpp
  test_wall_slope.cpp
  test_wall_survey.cpp
  test_survey_composition.cpp
  test_survey_sweep.cpp
  test_survey_cue.cpp
  test_depth_lift.cpp
  test_perf_report.cpp
  test_glyph_tables.cpp
  ../src/generated/GlyphTables.cpp
)
target_link_libraries(zonai_survey_pure_tests PRIVATE doctest::doctest)
target_include_directories(zonai_survey_pure_tests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/pure"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/presentation"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/generated"
  "${CMAKE_CURRENT_SOURCE_DIR}/../runtime-support/include"
)
target_compile_features(zonai_survey_pure_tests PRIVATE cxx_std_23)
if(MSVC)
  target_compile_options(zonai_survey_pure_tests PRIVATE /W4 /WX /permissive- /EHsc)
else()
  target_compile_options(zonai_survey_pure_tests PRIVATE -Wall -Wextra -Werror)
endif()

enable_testing()
include("${DOCTEST_CMAKE}")
doctest_discover_tests(zonai_survey_pure_tests)
