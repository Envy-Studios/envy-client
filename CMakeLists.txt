cmake_minimum_required(VERSION 3.22)

project(Envy)

set(ENVY_BUILD_TIMESTAMP "" CACHE STRING
        "Optional fixed Envy build timestamp in YYYY-MM-DD_HH-mm-ssZ UTC format")
if (ENVY_BUILD_TIMESTAMP AND NOT ENVY_BUILD_TIMESTAMP MATCHES
        "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]_[0-9][0-9]-[0-9][0-9]-[0-9][0-9]Z$")
    message(FATAL_ERROR
            "ENVY_BUILD_TIMESTAMP must use YYYY-MM-DD_HH-mm-ssZ UTC+0 format; got '${ENVY_BUILD_TIMESTAMP}'")
endif ()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel;Nightly" CACHE STRING "" FORCE)

# Nightly keeps RelWithDebInfo-style compilation so we get usable symbols, but
# links like a shipped binary so the DLL doesn't become a My 600-Lb. Life contestant
set(CMAKE_C_FLAGS_NIGHTLY "${CMAKE_C_FLAGS_RELWITHDEBINFO}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_NIGHTLY "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_NIGHTLY "${CMAKE_EXE_LINKER_FLAGS_RELEASE}" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_NIGHTLY "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}" CACHE STRING "" FORCE)

if (NOT MSVC)
    message(FATAL_ERROR "Compiler unsupported, must use MSVC")
endif()

# Enable Hot Reload / debug info format for supported MSVC generators
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug>:EditAndContinue>$<$<CONFIG:RelWithDebInfo,Nightly>:ProgramDatabase>")
endif ()

# MSVC runtime
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

# Add all sources in src/* and deps/*
file(GLOB_RECURSE Envy_SRC CONFIGURE_DEPENDS
        "src/*.h"
        "src/*.cpp"
        "deps/*.h"
        "deps/*.cpp"
)

# Assets
function(ADD_RESOURCES out_var)
    set(result)
    foreach(in_f ${ARGN})
        file(RELATIVE_PATH src_f ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${in_f})
        set(out_f "${PROJECT_BINARY_DIR}/${in_f}.o")

        add_custom_command(
                OUTPUT ${out_f}
                COMMAND ld.exe -r -b binary -o ${out_f} ${src_f}
                DEPENDS ${in_f}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                COMMENT "Building object ${out_f}"
                VERBATIM
        )

        list(APPEND result ${out_f})
    endforeach()

    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

# Add all files in assets/ as resources
file(GLOB_RECURSE RESOURCE_FILES
        RELATIVE ${CMAKE_CURRENT_LIST_DIR}
        CONFIGURE_DEPENDS
        "assets/*"
)

add_resources(EMBEDDED_RESOURCES ${RESOURCE_FILES})

set(ENVY_BUILD_TIMESTAMP_SOURCE
        "${PROJECT_BINARY_DIR}/generated/EnvyBuildTimestamp.cpp")
set_source_files_properties("${ENVY_BUILD_TIMESTAMP_SOURCE}" PROPERTIES GENERATED TRUE)

add_custom_target(EnvyBuildTimestampGenerator
        COMMAND ${CMAKE_COMMAND}
        "-DENVY_BUILD_TIMESTAMP=${ENVY_BUILD_TIMESTAMP}"
        "-DOUTPUT_FILE=${ENVY_BUILD_TIMESTAMP_SOURCE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBuildTimestamp.cmake"
        BYPRODUCTS "${ENVY_BUILD_TIMESTAMP_SOURCE}"
        COMMENT "Refreshing Envy UTC build timestamp"
        VERBATIM
)

add_library(Envy SHARED
        ${Envy_SRC}
        ${EMBEDDED_RESOURCES}
        "${ENVY_BUILD_TIMESTAMP_SOURCE}"
)
add_dependencies(Envy EnvyBuildTimestampGenerator)

# Final artifact names. Keep these in CMake so the DLL CodeView record and
# linker-generated PDB name agree, **do not rename the DLL/PDB after linking!!**
set_target_properties(Envy PROPERTIES
        OUTPUT_NAME_RELEASE "Envy"
        PDB_NAME_RELEASE "Envy"
        OUTPUT_NAME_RELWITHDEBINFO "Envy"
        PDB_NAME_RELWITHDEBINFO "Envy"
        OUTPUT_NAME_DEBUG "EnvyDebug"
        PDB_NAME_DEBUG "EnvyDebug"
        OUTPUT_NAME_NIGHTLY "EnvyNightly"
        PDB_NAME_NIGHTLY "EnvyNightly"
)

# C++ standard
set_property(TARGET Envy PROPERTY CXX_STANDARD 23)
set_property(TARGET Envy PROPERTY CXX_STANDARD_REQUIRED ON)

# Compile definitions
target_compile_definitions(Envy PRIVATE
        _UNICODE
        UNICODE
        _HAS_STATIC_RTTI=0

        $<$<CONFIG:Debug>:ENVY_DEBUG>
        $<$<CONFIG:Debug>:ENVY_CRASH_REPORTING>
        $<$<CONFIG:Release>:ENVY_RELEASE>
        $<$<CONFIG:Release>:ENVY_CRASH_REPORTING>
        $<$<CONFIG:RelWithDebInfo>:ENVY_RELEASE>
        $<$<CONFIG:RelWithDebInfo>:ENVY_CRASH_REPORTING>
        $<$<CONFIG:MinSizeRel>:ENVY_RELEASE>
        $<$<CONFIG:Nightly>:ENVY_NIGHTLY>
        $<$<CONFIG:Nightly>:ENVY_CRASH_REPORTING>
)

# Compile options
target_compile_options(Envy PRIVATE
        /EHsc
        /GR-
        /W4
)

target_link_options(Envy PRIVATE
        $<$<CONFIG:Release>:/INCREMENTAL:NO>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>

        $<$<CONFIG:Nightly>:/DEBUG:FULL>
        $<$<CONFIG:Nightly>:/INCREMENTAL:NO>
        $<$<CONFIG:Nightly>:/OPT:REF>
        $<$<CONFIG:Nightly>:/OPT:NOICF>
)

target_include_directories(Envy PRIVATE
        src
        deps
)

# Link libraries
target_link_libraries(Envy PRIVATE
        # WinRT
        WindowsApp.lib
        Shlwapi.lib

        # Windows 10 fallback file picker
        Comdlg32.lib

        # Needed for getting app version
        Version.lib

        # Needed for DirectX hooking and Direct2D rendering
        d3d11
        d3d12
        dxguid
        dxgi

        # UI rendering
        d2d1

        # Text rendering
        dwrite

        # ImageNtHeader for mnemosyne
        Dbghelp

        # PlaySound for scripting
        winmm

        # discord oauth loopback listener
        ws2_32

        # dpapi session storage
        crypt32
)

add_subdirectory(src)
add_subdirectory(deps)
