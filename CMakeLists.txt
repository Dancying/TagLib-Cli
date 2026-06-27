cmake_minimum_required(VERSION 3.10)

if(NOT PROJECT_VERSION)
    set(PROJECT_VERSION "1.0.0")
endif()

project(TagLibCLI VERSION ${PROJECT_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib:$ORIGIN")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
)

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(taglib-cli ${SOURCES})

target_include_directories(taglib-cli PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

find_library(TAGLIB_LIB NAMES tag REQUIRED)

target_link_libraries(taglib-cli PRIVATE 
    ${TAGLIB_LIB}
)
