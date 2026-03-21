cmake_minimum_required(VERSION 3.19)

# project name
project(cpu-rasterizer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_BUILD_TYPE Release)
set(CXX_FLAGS "-Wall")

# header files
file(GLOB HEADER_FILES
    ${CMAKE_SOURCE_DIR}/src/*.hpp
)

# source files
file(GLOB SORUCES_FILES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

# Create exec file
add_executable(${PROJECT_NAME}
    ${HEADER_FILES}
    ${SORUCES_FILES}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    CPURASTERIZER_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    CPURASTERIZER_RESOURCE_DIR="${CMAKE_SOURCE_DIR}/resources"
)

# Settings for platform
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Windows specific settings (Visual Studio)
    message(STATUS "Configuring for Windows (Visual Studio)")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/wd4819>")

    # Windows include path
    include_directories(
        ${CMAKE_SOURCE_DIR}/lib/SDL2/Windows/include
        ${CMAKE_SOURCE_DIR}/src
    )

    # Windows link path
    link_directories(
        ${CMAKE_SOURCE_DIR}/lib/SDL2/Windows/lib/x86
        ${CMAKE_SOURCE_DIR}/lib/SDL2/Windows/lib/x64
    )

    # Detect architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8) # 64-bit
        message(STATUS "64-bit architecture detected")
        set(SDL_LIB_PATH "${CMAKE_SOURCE_DIR}/lib/SDL2/Windows/lib/x64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4) # 32-bit
        message(STATUS "32-bit architecture detected")
        set(SDL_LIB_PATH "${CMAKE_SOURCE_DIR}/lib/SDL2/Windows/lib/x86")
    else()
        message(FATAL_ERROR "Unknown architecture detected: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    set(SDL_DLL_PATH "${SDL_LIB_PATH}/SDL2.dll")

    # Define imported libraries based on architecture
    add_library(lib-sdl2 STATIC IMPORTED)
    set_target_properties(lib-sdl2
        PROPERTIES IMPORTED_LOCATION "${SDL_LIB_PATH}/SDL2.lib"
    )

    add_library(lib-sdl2main STATIC IMPORTED)
    set_target_properties(lib-sdl2main
        PROPERTIES IMPORTED_LOCATION "${SDL_LIB_PATH}/SDL2main.lib"
    )

    # Link the SDL2 libraries
    target_link_libraries(${PROJECT_NAME}
        lib-sdl2
        lib-sdl2main
    )

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL_DLL_PATH}" $<TARGET_FILE_DIR:${PROJECT_NAME}>)

elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # macOS specific settings (Xcode)
    message(STATUS "Configuring for macOS (Xcode)")

    # macOS include path
    include_directories(
        ${CMAKE_SOURCE_DIR}/lib/SDL2/macOS/include
        ${CMAKE_SOURCE_DIR}/src
    )

    link_directories(
        ${CMAKE_SOURCE_DIR}/lib/SDL2/macOS/lib/arm64
        ${CMAKE_SOURCE_DIR}/lib/SDL2/macOS/lib/x86_64
    )

    # Find macOS frameworks
    find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
    find_library(FOUNDATION Foundation REQUIRED)
    find_library(CORE_AUDIO CoreAudio REQUIRED)
    find_library(CORE_AUDIO_KIT CoreAudioKit REQUIRED)
    find_library(IO_KIT IOKit REQUIRED)
    find_library(CORE_GRAPHICS CoreGraphics REQUIRED)
    find_library(METAL_KIT MetalKit REQUIRED)
    find_library(AUDIO_TOOLBOX AudioToolbox REQUIRED)
    find_library(CORE_HAPTICS CoreHaptics REQUIRED)
    find_library(CORE_VIDEO CoreVideo REQUIRED)
    find_library(FORCE_FEEDBACK ForceFeedback REQUIRED)
    find_library(CARBON Carbon REQUIRED)
    find_library(GAME_CONTROLLER GameController REQUIRED)  # GameController 추가
    find_library(METAL Metal REQUIRED)  # Metal 추가
    find_library(APPKIT AppKit REQUIRED)  # AppKit 추가

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
        message(STATUS "Intel/AMD CPU detected")
        # Intel/AMD specific settings
        # x86_64
        add_library(lib-sdl2 STATIC IMPORTED)
        set_target_properties(lib-sdl2
          PROPERTIES IMPORTED_LOCATION
          ${CMAKE_SOURCE_DIR}/lib/SDL2/macOS/lib/x86_64/libSDL2.a)

        add_library(lib-sdl2main STATIC IMPORTED)
        set_target_properties(lib-sdl2main
            PROPERTIES IMPORTED_LOCATION
            ${CMAKE_SOURCE_DIR}/lib/SDL2/macOS/lib/x86_64/libSDL2main.a)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        message(STATUS "ARM CPU detected")
        # ARM specific settings
        add_library(lib-sdl2 STATIC IMPORTED)
        set_target_properties(lib-sdl2
            PROPERTIES IMPORTED_LOCATION
            ${CMAKE_SOURCE_DIR}/lib/SDL2/macOS/lib/arm64/libSDL2.a)

        add_library(lib-sdl2main STATIC IMPORTED)
        set_target_properties(lib-sdl2main
            PROPERTIES IMPORTED_LOCATION
            ${CMAKE_SOURCE_DIR}/lib/SDL2/macOS/lib/arm64/libSDL2main.a)
    else()
        message(STATUS "Unknown CPU architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    target_link_libraries(${PROJECT_NAME}
        lib-sdl2
        lib-sdl2main
        ${SDL2_LIBRARIES}
        ${CORE_FOUNDATION}
        ${FOUNDATION}
        ${CORE_AUDIO}
        ${CORE_AUDIO_KIT}
        ${IO_KIT}
        ${CORE_GRAPHICS}
        ${METAL_KIT}
        ${AUDIO_TOOLBOX}
        ${CORE_HAPTICS}
        ${CORE_VIDEO}  # CoreVideo 링크
        ${FORCE_FEEDBACK}  # ForceFeedback 링크
        ${CARBON}  # Carbon 링크
        ${GAME_CONTROLLER}  # GameController 링크
        ${METAL}  # Metal 링크
        ${APPKIT}  # AppKit 링크        
    )
endif()

# Keep resources available beside the built executable (including Xcode/DerivedData paths).
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources")

# SDL2 DLL copy is handled in the Windows block above.
