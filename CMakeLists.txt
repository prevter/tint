cmake_minimum_required(VERSION 3.21)
project(tint LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TINT_BUILD_TNTUTIL "Build the tntutil executable" OFF)

add_library(
    ${PROJECT_NAME} STATIC
    src/Atlas.cpp
    src/AtlasGroup.cpp
    src/Font.cpp
    src/Shaper.cpp
    src/TntFont.cpp
)

if (NOT COMMAND CPMAddPackage AND (NOT TARGET freetype OR NOT TARGET harfbuzz OR NOT TARGET lz4))
    include(cmake/CPM.cmake)
endif()

if (NOT TARGET freetype)
    CPMAddPackage("gh:freetype/freetype#99b479d")
endif()

if (NOT TARGET harfbuzz)
    CPMAddPackage("gh:harfbuzz/harfbuzz#13.2.1")
endif()

if (NOT TARGET lz4)
    CPMAddPackage(
        URI "gh:lz4/lz4@1.10.0"
        DOWNLOAD_ONLY YES
    )
    file(GLOB LZ4_SOURCES ${lz4_SOURCE_DIR}/lib/*.c)
    add_library(lz4 STATIC ${LZ4_SOURCES})
    target_include_directories(lz4 PUBLIC ${lz4_SOURCE_DIR}/lib)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PUBLIC freetype harfbuzz lz4)

if (TINT_BUILD_TNTUTIL)
    add_subdirectory(tntutil)
endif()

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})