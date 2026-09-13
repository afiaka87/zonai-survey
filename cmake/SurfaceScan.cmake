set(FIDELITY_UAM "" CACHE FILEPATH "Pinned external NVN shader compiler")
set(FIDELITY_UAM_RUNTIME "" CACHE PATH "External compiler runtime DLL directory")
set(FIDELITY_NVDISASM "" CACHE FILEPATH "Pinned external Maxwell disassembler")
if(NOT EXISTS "${FIDELITY_UAM}" OR NOT EXISTS "${FIDELITY_NVDISASM}")
    message(FATAL_ERROR "Set FIDELITY_UAM and FIDELITY_NVDISASM; see DESIGN.md")
endif()
set(SURFACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(surface_shader_args)
if(SURVEY_CONSTRAINED OR SURVEY_TUNING)
    list(APPEND surface_shader_args --constrained)
endif()
if(NOT SURVEY_FIDELITY_PLAYGROUND)
    list(APPEND surface_shader_args --production)
    list(FILTER SURVEY_SOURCES EXCLUDE REGEX "/FidelityTrace\\.cpp$")
endif()
target_include_directories(subsdk9 PRIVATE "${SURFACE_ROOT}/src/surface" "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(SHADER_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/FidelityShaders.hpp")
add_custom_command(OUTPUT "${SHADER_HEADER}"
    COMMAND uv run --no-project python "${SURFACE_ROOT}/tools/compile_shaders.py"
        --uam "${FIDELITY_UAM}" --runtime-dir "${FIDELITY_UAM_RUNTIME}"
        --nvdisasm "${FIDELITY_NVDISASM}"
        --source "${SURFACE_ROOT}/shaders" --output "${CMAKE_CURRENT_BINARY_DIR}/generated" ${surface_shader_args}
    DEPENDS "${SURFACE_ROOT}/tools/compile_shaders.py"
        "${SURFACE_ROOT}/shaders/diagnostic.vert" "${SURFACE_ROOT}/shaders/diagnostic.frag"
        "${SURFACE_ROOT}/shaders/calibration.frag" "${SURFACE_ROOT}/shaders/uniform_check.frag"
        "${SURFACE_ROOT}/shaders/raw_depth.frag" "${SURFACE_ROOT}/shaders/aesthetic_math.inl"
        "${FIDELITY_UAM}" "${FIDELITY_NVDISASM}"
    VERBATIM)
add_custom_target(surface_shaders DEPENDS "${SHADER_HEADER}")
add_dependencies(subsdk9 surface_shaders)
