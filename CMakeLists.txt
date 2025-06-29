cmake_minimum_required(VERSION 3.26.0 FATAL_ERROR)

project(
    caulk
    VERSION 0.1.0
    DESCRIPTION "THE Steamworks API wrapper for use with plain C."
    LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(
    yyjson
    GIT_REPOSITORY "https://github.com/ibireme/yyjson.git"
    GIT_TAG "0.11.1")
FetchContent_MakeAvailable(yyjson)

# FetchContent_Declare(raw URL "https://partner.steamgames.com/downloads/steamworks_sdk_162.zip")
# FetchContent_MakeAvailable(raw)

if(NOT DEFINED STEAMWORKS_SDK_ZIP)
    set(STEAMWORKS_SDK_ZIP ${CMAKE_SOURCE_DIR}/steamworks_sdk_162.zip)
endif()

set(raw_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/steamworks)
file(ARCHIVE_EXTRACT INPUT ${STEAMWORKS_SDK_ZIP} DESTINATION ${raw_SOURCE_DIR})

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(RAW_ROOT ${raw_SOURCE_DIR}/sdk)
set(RAW_INCLUDE_DIR ${RAW_ROOT}/public)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(GEN_NAME ${PROJECT_NAME}GenBindings)
add_executable(${GEN_NAME} ${SRC_DIR}/gen.c)
target_link_libraries(${GEN_NAME} PRIVATE yyjson)

set(GEN_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/steamworksgen)
set(GEN_H_OUT ${GEN_OUT_DIR}/__gen.h)
set(GEN_C_OUT ${GEN_OUT_DIR}/__gen.cpp)
add_custom_command(
    TARGET ${GEN_NAME}
    POST_BUILD
    COMMAND $<TARGET_FILE:${GEN_NAME}> ARGS ${GEN_H_OUT} ${GEN_C_OUT} ${RAW_INCLUDE_DIR}/steam/steam_api.json
    BYPRODUCTS ${GEN_H_OUT} ${GEN_C_OUT}
    VERBATIM)

set(LIB_NAME ${PROJECT_NAME})
add_library(${LIB_NAME} ${SRC_DIR}/caulk.h ${GEN_H_OUT} ${GEN_C_OUT})
add_dependencies(${LIB_NAME} ${GEN_NAME})
set_target_properties(${LIB_NAME} PROPERTIES LINKER_LANGUAGE C)
target_compile_definitions(${LIB_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS=1)
target_include_directories(
    ${LIB_NAME}
    PUBLIC ${SRC_DIR}
    PRIVATE ${RAW_INCLUDE_DIR} ${RAW_INCLUDE_DIR}/steam
    INTERFACE ${GEN_OUT_DIR})

set(TEST_NAME ${PROJECT_NAME}Test)
add_executable(${TEST_NAME} ${SRC_DIR}/test.c)
target_link_libraries(${TEST_NAME} ${LIB_NAME})

if(${CMAKE_SIZEOF_VOID_P} STREQUAL 4) # 32 bitches...
    if(WIN32)

    elseif(LINUX)

    else()

    endif()
else()
    if(WIN32)

    elseif(LINUX)

    else()

    endif()
endif()
