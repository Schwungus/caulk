cmake_minimum_required(VERSION 3.24.0 FATAL_ERROR)

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

set(STEAMWORKS_SDK_ZIP
    ${CMAKE_SOURCE_DIR}/steamworks_sdk_162.zip
    CACHE STRING "Path to Steamworks SDK (zip)")
if(NOT EXISTS ${STEAMWORKS_SDK_ZIP})
    message(FATAL_ERROR "You forgot to specify a path to Steamworks SDK .zip! Review caulk's README")
endif()

set(STEAM_APPID
    ${CMAKE_SOURCE_DIR}/steam_appid.txt
    CACHE STRING "Path to steam_appid.txt")
if(NOT EXISTS ${STEAM_APPID})
    message(FATAL_ERROR "You forgot to add a steam_appid.txt! Just write \"480\" inside it.")
endif()

set(SDK_SOURCE_DIR ${CMAKE_BINARY_DIR}/steamworks)
file(ARCHIVE_EXTRACT INPUT ${STEAMWORKS_SDK_ZIP} DESTINATION ${SDK_SOURCE_DIR})

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CAULK_LIB_NAME
    ${PROJECT_NAME}
    CACHE INTERNAL "caulk library target name")
set(SDK_ROOT ${SDK_SOURCE_DIR}/sdk)
set(SDK_INCLUDE_DIR ${SDK_ROOT}/public)
set(CAULK_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(GEN_NAME ${CAULK_LIB_NAME}GenBindings)
add_executable(${GEN_NAME} ${CAULK_SRC_DIR}/caulk-gen.c)
target_link_libraries(${GEN_NAME} PRIVATE yyjson)

set(GEN_OUT_DIR ${CMAKE_BINARY_DIR}/steamworksgen)
file(MAKE_DIRECTORY ${GEN_OUT_DIR})

set(GEN_H_OUT ${GEN_OUT_DIR}/__gen.h)
set(GEN_C_OUT ${GEN_OUT_DIR}/__gen.cpp)
add_custom_command(
    TARGET ${GEN_NAME}
    POST_BUILD
    COMMAND $<TARGET_FILE:${GEN_NAME}> ${GEN_H_OUT} ${GEN_C_OUT} ${SDK_INCLUDE_DIR}/steam/steam_api.json
    BYPRODUCTS ${GEN_H_OUT} ${GEN_C_OUT})

set(DYLIB_ROOT ${SDK_ROOT}/redistributable_bin)
if(WIN32)
    if(${CMAKE_SIZEOF_VOID_P} STREQUAL 4) # 32 bitches...
        set(STEAM_API_PATH ${DYLIB_ROOT})
        set(STEAM_API_NAME steam_api)
    else()
        set(STEAM_API_PATH ${DYLIB_ROOT}/win64)
        set(STEAM_API_NAME steam_api64)
    endif()

    set(CAULK_COPY_DLL
        ${STEAM_API_PATH}/${STEAM_API_NAME}.dll
        CACHE INTERNAL "")
elseif(UNIX)
    set(STEAM_API_NAME steam_api)
    if(${CMAKE_SIZEOF_VOID_P} STREQUAL 4) # 32 bitches...
        set(STEAM_API_PATH ${DYLIB_ROOT}/linux32)
        set(STEAM_API_NAME steam_api)
    else()
        set(STEAM_API_PATH ${DYLIB_ROOT}/linux64)
    endif()

    set(CAULK_COPY_DLL
        ${STEAM_API_PATH}/libsteam_api.so
        CACHE INTERNAL "")
else()
    set(STEAM_API_PATH ${DYLIB_ROOT}/osx)
    set(STEAM_API_NAME steam_api)
    set(CAULK_COPY_DLL
        ${STEAM_API_PATH}/libsteam_api.dylib
        CACHE INTERNAL "")
endif()

find_library(
    STEAM_API_LIBRARY
    NAMES ${STEAM_API_NAME}
    PATHS ${STEAM_API_PATH}
    NO_DEFAULT_PATH)

add_library(${CAULK_LIB_NAME} ${GEN_C_OUT} ${CAULK_SRC_DIR}/caulk.cpp)
add_dependencies(${CAULK_LIB_NAME} ${GEN_NAME})
set_target_properties(${CAULK_LIB_NAME} PROPERTIES LINKER_LANGUAGE C)
target_compile_definitions(${CAULK_LIB_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS=1)
target_include_directories(
    ${CAULK_LIB_NAME}
    INTERFACE $<BUILD_INTERFACE:${CAULK_SRC_DIR}>
    PUBLIC ${GEN_OUT_DIR}
    PRIVATE ${SDK_INCLUDE_DIR}/steam ${SDK_INCLUDE_DIR})
target_link_libraries(${CAULK_LIB_NAME} PUBLIC ${STEAM_API_LIBRARY})

if(DEFINED CAULK_COPY_DLL)
    add_custom_command(
        TARGET ${CAULK_LIB_NAME}
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CAULK_COPY_DLL} ${CMAKE_CURRENT_BINARY_DIR})
endif()

function(caulk_register TGT)
    target_link_libraries(${TGT} ${CAULK_LIB_NAME})

    if(DEFINED CAULK_COPY_DLL)
        add_custom_command(
            TARGET ${TGT}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${CAULK_COPY_DLL} $<TARGET_FILE_DIR:${TGT}>)
    endif()

    add_custom_command(
        TARGET ${TGT}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${STEAM_APPID} $<TARGET_FILE_DIR:${TGT}>)
endfunction()

set(TEST_NAME ${CAULK_LIB_NAME}Test)
option(CAULK_BUILD_TEST "Build the ${TEST_NAME} executable?" ON)
if(CAULK_BUILD_TEST)
    add_executable(${TEST_NAME} ${CAULK_SRC_DIR}/caulk-test.c)
    set_target_properties(${TEST_NAME} PROPERTIES LINKER_LANGUAGE C)
    caulk_register(${TEST_NAME})
endif()
