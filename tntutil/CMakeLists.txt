cmake_minimum_required(VERSION 3.21)
project(tntutil)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT TARGET slic)
    CPMAddPackage("gh:prevter/slic@1.0.1")
endif()

if (NOT TARGET msdf-atlas-gen)
    add_library(Freetype::Freetype ALIAS freetype)
    find_package(msdf-atlas-gen CONFIG REQUIRED)
endif()

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC tint slic msdf-atlas-gen msdfgen-ext msdfgen-core)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${msdf-atlas-gen_SOURCE_DIR}
    ${msdf-atlas-gen_SOURCE_DIR}/msdfgen
)