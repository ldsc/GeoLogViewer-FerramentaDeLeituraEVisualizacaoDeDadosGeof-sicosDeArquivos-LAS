cmake_minimum_required(VERSION 3.26)
project(WellLogVisualizer)

set(CMAKE_CXX_STANDARD 23)

# Diretórios
include_directories(src)
include_directories(src/core)
include_directories(src/plotting)
include_directories(src/interpretation)

# Arquivos-fonte
file(GLOB_RECURSE SOURCES
    src/*.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})
