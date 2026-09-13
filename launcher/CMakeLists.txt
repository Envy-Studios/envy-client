# EnvyLauncher - a small exe that carries the client dll inside itself,
# starts Minecraft and injects it.

# The dll gets copied out after it links, then turned into a linkable object
# with the same ld.exe trick the asset embedder uses, and linked straight into
# the exe. No .rc resource compiler involved, and the bytes are the exact dll
# this build produced.
set(ENVY_EMBED_DIR "${CMAKE_BINARY_DIR}/launcher-embed")
set(ENVY_EMBED_DLL "${ENVY_EMBED_DIR}/Envy.dll")
set(ENVY_EMBED_OBJ "${ENVY_EMBED_DIR}/Envy_dll.o")

add_custom_command(
        OUTPUT "${ENVY_EMBED_DLL}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ENVY_EMBED_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Envy>" "${ENVY_EMBED_DLL}"
        DEPENDS Envy
        COMMENT "Staging Envy.dll for the launcher"
        VERBATIM
)

add_custom_command(
        OUTPUT "${ENVY_EMBED_OBJ}"
        COMMAND ld.exe -r -b binary -o "${ENVY_EMBED_OBJ}" "Envy.dll"
        DEPENDS "${ENVY_EMBED_DLL}"
        WORKING_DIRECTORY "${ENVY_EMBED_DIR}"
        COMMENT "Baking Envy.dll into the launcher"
        VERBATIM
)

add_executable(EnvyLauncher
        src/Main.cpp
        src/Flow.cpp
        src/AuthStore.cpp
        src/DiscordOAuth.cpp
        src/ProKeyClient.cpp
        src/Http.cpp
        src/Injector.cpp
        "${ENVY_EMBED_OBJ}"
)

set_source_files_properties("${ENVY_EMBED_OBJ}" PROPERTIES GENERATED TRUE)
add_dependencies(EnvyLauncher Envy)

# ships as Envy.exe next to the dll (out/build/<preset>/Envy.exe)
set_target_properties(EnvyLauncher PROPERTIES
        OUTPUT_NAME "Envy"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        WIN32_EXECUTABLE TRUE
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        MSVC_RUNTIME_LIBRARY "MultiThreaded"
)

target_compile_definitions(EnvyLauncher PRIVATE
        _UNICODE
        UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

target_compile_options(EnvyLauncher PRIVATE
        /EHsc
        /W3
)

target_include_directories(EnvyLauncher PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(EnvyLauncher PRIVATE
        user32
        gdi32
        shell32
        advapi32
        ole32
        crypt32
        winhttp
        ws2_32
        comctl32
        nlohmann_json::nlohmann_json
)
