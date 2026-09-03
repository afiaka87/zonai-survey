# Adapted from https://github.com/vbe0201/switch-cmake

if (NOT SWITCH)
    message(FATAL_ERROR "This helper can only be used when cross-compiling for the Switch")
endif ()

set(__SWITCH_TOOLS_DIR ${PROJECT_SOURCE_DIR}/cmake)

macro(find_tool tool)
    if (NOT ${tool})
        find_program(${tool} ${tool})
        if (${tool})
            message(STATUS "${tool} - found")
        else ()
            message(WARNING "${tool} - not found")
        endif ()
    endif ()
endmacro()

find_tool(elf2kip)

find_tool(elf2nro)

find_tool(elf2nso)

find_tool(nacptool)

find_tool(npdmtool)

find_tool(nxlink)

find_tool(build_pfs0)

find_tool(build_romfs)

find_tool(bin2s)

macro(set_app_title title)
    if ("${title}" STREQUAL "title-NOTFOUND")
        set(__HOMEBREW_APP_TITLE "${CMAKE_PROJECT_NAME}")
        message(WARNING "The title of the application is unspecified")
    else ()
        set(__HOMEBREW_APP_TITLE ${title})
    endif ()
endmacro()

macro(set_app_author author)
    if ("${author}" STREQUAL "author-NOTFOUND")
        set(__HOMEBREW_APP_AUTHOR "Unspecified author")
        message(WARNING "The author of the application is unspecified")
    else ()
        set(__HOMEBREW_APP_AUTHOR ${author})
    endif ()
endmacro()

macro(set_app_version version)
    if ("${version}" STREQUAL "version-NOTFOUND")
        set(__HOMEBREW_APP_VERSION "1.0.0")
        message(WARNING "The version of the application is unspecified")
    else ()
        set(__HOMEBREW_APP_VERSION ${version})
    endif ()
endmacro()

macro(set_app_icon file)
    if (NOT NO_ICON)
        if (EXISTS ${file})
            set(__HOMEBREW_ICON ${file})
        elseif (EXISTS ${PROJECT_SOURCE_DIR}/icon.jpg)
            set(__HOMEBREW_ICON ${PROJECT_SOURCE_DIR}/icon.jpg)
        elseif (LIBNX)
            set(__HOMEBREW_ICON ${LIBNX}/default_icon.jpg)
        else ()
            message(WARNING "Failed to resolve application icon")
        endif ()
    endif ()
endmacro()

macro(set_app_json file)
    if (EXISTS ${file})
        set(__HOMEBREW_JSON_CONFIG ${file})
    elseif (EXISTS ${PROJECT_SOURCE_DIR}/config.json)
        set(__HOMEBREW_JSON_CONFIG ${PROJECT_SOURCE_DIR}/config.json)
    else ()
        message(WARNING "Failed to resolve the JSON config")
    endif ()
endmacro()

macro(__add_binary_library target)
    if (NOT ${ARGC} GREATER 1)
        message(FATAL_ERROR "No input files provided")
    endif ()

    get_cmake_property(ENABLED_LANGUAGES ENABLED_LANGUAGES)
    if (NOT ENABLED_LANGUAGES MATCHES ".*ASM.*")
        message(FATAL_ERROR "To use this macro, call enable_language(ASM) first")
    endif ()

    foreach (__file ${ARGN})
        get_filename_component(__file_name ${__file} NAME)
        string(REGEX REPLACE "^([0-9])" "_\\1" __BINARY_FILE ${__file_name})
        string(REGEX REPLACE "[-./]" "_" __BINARY_FILE ${__BINARY_FILE})

        configure_file(${__SWITCH_TOOLS_DIR}/bin2s_header.h.in ${CMAKE_CURRENT_BINARY_DIR}/bin2s_include/${__BINARY_FILE}.h)
    endforeach ()

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin2s_lib)
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/bin2s_lib/${target}.s
            COMMAND ${bin2s} ${ARGN} > ${CMAKE_CURRENT_BINARY_DIR}/bin2s_lib/${target}.s
            DEPENDS ${ARGN}
            WORKING_DIRECTORY ..
    )

    add_library(${target} ${CMAKE_CURRENT_BINARY_DIR}/bin2s_lib/${target}.s)
    target_include_directories(${target} INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/bin2s_include)
endmacro()

function(target_embed_binaries target)
    if (NOT ${ARGC} GREATER 1)
        message(FATAL_ERROR "No input files provided")
    endif ()

    get_filename_component(__1st_bin_file ${ARGV1} NAME)
    __add_binary_library(__${target}_embed_${__1st_bin_file} ${ARGN})
    target_link_libraries(${target} __${target}_embed_${__1st_bin_file})
endfunction()

function(__generate_nacp target)
    get_filename_component(target_we ${target} NAME_WE)

    get_target_property(title ${target} "APP_TITLE")
    get_target_property(author ${target} "APP_AUTHOR")
    get_target_property(version ${target} "APP_VERSION")
    get_target_property(title_id ${target} "TITLE_ID")

    set_app_title(${title})
    set_app_author(${author})
    set_app_version(${version})

    if (NOT "${title_id}" STREQUAL "")
        set(NACPFLAGS "--titleid=\"${title_id}\"")
    else ()
        set(NACPFLAGS "")  # Purposefully empty.
    endif ()

    add_custom_target(create_nacp
            COMMAND ${nacptool} --create ${__HOMEBREW_APP_TITLE} ${__HOMEBREW_APP_AUTHOR} ${__HOMEBREW_APP_VERSION} ${target_we}.nacp ${NACPFLAGS}
            DEPENDS ${target}
            VERBATIM
            )
endfunction()

function(__generate_npdm target)
    get_filename_component(target_we ${target} NAME_WE)

    get_target_property(config_json ${target} "CONFIG_JSON")

    set_app_json(${config_json})

    if (NOT __HOMEBREW_JSON_CONFIG)
        message(FATAL_ERROR "Cannot generate a NPDM file without the \"CONFIG_JSON\" property being set for the target")
    endif ()

    add_custom_target(create_npdm ALL
            COMMAND ${npdmtool} ${__HOMEBREW_JSON_CONFIG} ${CMAKE_CURRENT_BINARY_DIR}/main.npdm
            DEPENDS ${target} ${__HOMEBREW_JSON_CONFIG}
            VERBATIM
            )
endfunction()

function(add_nro_target target)
    get_filename_component(target_we ${target} NAME_WE)

    get_target_property(icon ${target} "ICON")
    get_target_property(romfs ${target} "ROMFS")

    set_app_icon(${icon})

    set(NROFLAGS "")

    if (__HOMEBREW_ICON)
        string(APPEND NROFLAGS "--icon=${__HOMEBREW_ICON}")
    endif ()

    if (NOT "${romfs}" STREQUAL "romfs-NOTFOUND")
        if (IS_DIRECTORY ${romfs})
            string(APPEND NROFLAGS " --romfsdir=${romfs}")
        else ()
            if (EXISTS ${romfs})
                string(APPEND NROFLAGS " --romfs=${romfs}")
            else ()
                message(WARNING "The provided RomFS image at ${romfs} doesn't exist")
            endif ()
        endif ()
    endif ()

    if (NOT NO_NACP)
        __generate_nacp(${target})

        add_custom_command(
                OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.nro
                COMMAND ${elf2nro} $<TARGET_FILE:${target}> ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.nro --nacp=${CMAKE_CURRENT_BINARY_DIR}/${target_we}.nacp ${NROFLAGS}
                DEPENDS ${target} ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.nacp
                VERBATIM
        )
    else ()
        message(STATUS "No .nacp file will be generated for ${target_we}.nro")

        add_custom_command(
                OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.nro
                COMMAND ${elf2nro} $<TARGET_FILE:${target}> ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.nro ${NROFLAGS}
                DEPENDS ${target}
                VERBATIM
        )
    endif ()

    add_custom_target(${target_we}_nro ALL SOURCES ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.nro)
    set_target_properties(${target} PROPERTIES LINK_FLAGS "-specs=${LIBNX}/switch.specs")
endfunction()

function(add_nso_target target)
    add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/${target}.nso
            COMMAND ${elf2nso} $<TARGET_FILE:${target}> ${CMAKE_CURRENT_BINARY_DIR}/${target}.nso
            DEPENDS ${target}
            VERBATIM
    )

    add_custom_target(${target}_nso ALL SOURCES ${CMAKE_CURRENT_BINARY_DIR}/${target}.nso)
    set_target_properties(${target} PROPERTIES LINK_FLAGS "-specs=${LIBNX}/switch.specs")
endfunction()

function(add_nso_target_subsdk target)
    add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/${target}
            COMMAND ${elf2nso} $<TARGET_FILE:${target}> ${CMAKE_CURRENT_BINARY_DIR}/${target}
            DEPENDS ${target}
            VERBATIM
    )

    add_custom_target(${target}_nso ALL SOURCES ${CMAKE_CURRENT_BINARY_DIR}/${target})
    set_target_properties(${target} PROPERTIES LINK_FLAGS "-specs=${LIBNX}/switch.specs")
endfunction()

function(add_nsp_target target)
    get_filename_component(target_we ${target} NAME_WE)

    __generate_npdm(${target})

    if (NOT TARGET ${target_we}_nso)
        add_nso_target(${target})
    endif ()

    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.nsp
            PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/exefs
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.nso ${CMAKE_CURRENT_BINARY_DIR}/exefs/main
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.npdm ${CMAKE_CURRENT_BINARY_DIR}/exefs/main.npdm
            COMMAND ${build_pfs0} ${CMAKE_CURRENT_BINARY_DIR}/exefs ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.nsp
            DEPENDS ${target} ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.nso ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.npdm
            VERBATIM
    )

    add_custom_target(${target_we}_nsp ALL SOURCES ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.nsp)
    set_target_properties(${target} PROPERTIES LINK_FLAGS "-specs=${LIBNX}/switch.specs")
endfunction()

function(add_kip_target target)
    get_filename_component(target_we ${target} NAME_WE)

    get_target_property(config_json ${target} "CONFIG_JSON")

    set_app_json(${config_json})

    if (NOT __HOMEBREW_JSON_CONFIG)
        message(FATAL_ERROR "Cannot generate a KIP file without the \"CONFIG_JSON\" property being set for the target")
    endif ()

    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.kip
            COMMAND ${elf2kip} $<TARGET_FILE:${target}> ${__HOMEBREW_JSON_CONFIG} ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.kip
            DEPENDS ${target} ${__HOMEBREW_JSON_CONFIG}
            VERBATIM
    )

    add_custom_target(${target_we}_kip ALL SOURCES ${CMAKE_CURRENT_BINARY_DIR}/${target_we}.kip)
    set_target_properties(${target} PROPERTIES LINK_FLAGS "-specs=${LIBNX}/switch.specs")
endfunction()