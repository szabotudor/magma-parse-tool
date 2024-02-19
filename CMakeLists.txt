cmake_minimum_required(VERSION 3.27)
project(MPT)


set(
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test.cpp
)

add_executable(MPT ${SOURCES})

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    target_compile_definitions(MPT PRIVATE DEBUG)
elseif(${CMAKE_BUILD_TYPE} STREQUAL "Release")
    target_compile_definitions(MPT PRIVATE NDEBUG)
endif()
