cmake_minimum_required(VERSION 3.16)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(backend
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
include(CTest)

# -----------------------------
# Protobuf
# -----------------------------
find_package(Protobuf REQUIRED)

set(PROTO_SRC_DIR ${CMAKE_SOURCE_DIR}/proto)
set(PROTO_FILE ${PROTO_SRC_DIR}/FlightData.proto)

add_library(proto STATIC ${PROTO_FILE})

protobuf_generate(
    TARGET proto
    PROTOS ${PROTO_FILE}
)

target_link_libraries(proto
    PUBLIC
        protobuf::libprotobuf
)

target_include_directories(proto
    PUBLIC
        ${CMAKE_BINARY_DIR}
)

# -----------------------------
# Backend sources
# -----------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Network)


file(GLOB_RECURSE BACKEND_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_executable(backend
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${BACKEND_SOURCES}
)

target_include_directories(backend
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(backend PRIVATE _USE_MATH_DEFINES)

target_include_directories(backend PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(backend
    PRIVATE
        proto
        Qt6::Core 
        Qt6::Network
)

# -----------------------------
# Testing (Catch2)
# -----------------------------
enable_testing()

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.4
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(Catch2)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
)

add_executable(backend_tests
    ${TEST_SOURCES}
    ${BACKEND_SOURCES}
)

target_compile_definitions(backend_tests PRIVATE _USE_MATH_DEFINES)

target_include_directories(backend_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(backend_tests
    PRIVATE
        Catch2::Catch2WithMain
        proto
        Qt6::Core 
        Qt6::Network
)

add_test(NAME backend_tests COMMAND backend_tests)

# -----------------------------
# Redis
# -----------------------------
FetchContent_Declare(
    hiredis
    GIT_REPOSITORY https://github.com/redis/hiredis.git
    GIT_TAG v1.3.0
    GIT_SHALLOW TRUE
)

set(DISABLE_TESTS ON CACHE BOOL "Disable hiredis tests" FORCE)
set(ENABLE_EXAMPLES OFF CACHE BOOL "Disable hiredis examples/tests" FORCE)

FetchContent_MakeAvailable(hiredis)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/hiredis_include/hiredis)

file(COPY
    ${hiredis_SOURCE_DIR}/hiredis.h
    ${hiredis_SOURCE_DIR}/read.h
    ${hiredis_SOURCE_DIR}/sds.h
    ${hiredis_SOURCE_DIR}/async.h
    ${hiredis_SOURCE_DIR}/alloc.h
    DESTINATION ${CMAKE_BINARY_DIR}/hiredis_include/hiredis
)

set(HIREDIS_HEADER ${CMAKE_BINARY_DIR}/hiredis_include)
set(HIREDIS_LIB hiredis)

if(TARGET hiredis AND NOT TARGET hiredis::hiredis)
    add_library(hiredis::hiredis ALIAS hiredis)
endif()

FetchContent_Declare(
    redis-plus-plus
    GIT_REPOSITORY https://github.com/sewenew/redis-plus-plus.git
    GIT_TAG 1.3.9
    GIT_SHALLOW TRUE
)

set(REDIS_PLUS_PLUS_BUILD_TEST OFF CACHE BOOL "Disable building redis-plus-plus tests" FORCE)

FetchContent_MakeAvailable(redis-plus-plus)

target_link_libraries(backend PRIVATE redis++::redis++)
target_link_libraries(backend_tests PRIVATE redis++::redis++)