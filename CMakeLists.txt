cmake_minimum_required(VERSION 3.16)
project(bfly_test)

set(CMAKE_CXX_STANDARD 17)

# Spinnaker SDK
set(SPINNAKER_DIR "/opt/spinnaker")
if(NOT EXISTS "${SPINNAKER_DIR}/include/Spinnaker.h")
    message(FATAL_ERROR "Spinnaker SDK not found at ${SPINNAKER_DIR}")
endif()

add_library(spinnaker SHARED IMPORTED)
set_target_properties(spinnaker PROPERTIES
    IMPORTED_LOCATION ${SPINNAKER_DIR}/lib/libSpinnaker.so
    INTERFACE_INCLUDE_DIRECTORIES ${SPINNAKER_DIR}/include
)

# OpenCV
find_package(OpenCV REQUIRED)

# spdlog / fmt
# Set SPDLOG_INCLUDE_DIR if spdlog is not installed system-wide
# e.g. cmake -DSPDLOG_INCLUDE_DIR=/path/to/spdlog/include ..
if(NOT DEFINED SPDLOG_INCLUDE_DIR)
    set(SPDLOG_INCLUDE_DIR "/opt/spdlog/include")
endif()
find_package(fmt REQUIRED)

add_executable(bfly_test main.cpp)

# spdlog compiled sources
target_sources(bfly_test PRIVATE
    ${SPDLOG_INCLUDE_DIR}/../src/spdlog.cpp
    ${SPDLOG_INCLUDE_DIR}/../src/async.cpp
    ${SPDLOG_INCLUDE_DIR}/../src/file_sinks.cpp
    ${SPDLOG_INCLUDE_DIR}/../src/stdout_sinks.cpp
    ${SPDLOG_INCLUDE_DIR}/../src/color_sinks.cpp
    ${SPDLOG_INCLUDE_DIR}/../src/cfg.cpp
)

target_compile_definitions(bfly_test PRIVATE SPDLOG_COMPILED_LIB SPDLOG_FMT_EXTERNAL)

target_include_directories(bfly_test PRIVATE
    ${SPDLOG_INCLUDE_DIR}
)

target_link_libraries(bfly_test PRIVATE
    ${OpenCV_LIBS}
    spinnaker
    fmt::fmt
)
