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

set(GEN_GLUE_OUT ${GEN_OUT_DIR}/__gen.h)
set(GEN_H_OUT ${GEN_OUT_DIR}/caulk.h)
set(GEN_C_OUT ${GEN_OUT_DIR}/__gen.cpp)

add_custom_command(
    TARGET ${GEN_NAME}
    POST_BUILD
    COMMAND $<TARGET_FILE:${GEN_NAME}> ${GEN_GLUE_OUT} ${GEN_H_OUT} ${GEN_C_OUT} ${SDK_INCLUDE_DIR}/steam/steam_api.json
    BYPRODUCTS ${GEN_GLUE_OUT} ${GEN_H_OUT} ${GEN_C_OUT})

add_library(${CAULK_LIB_NAME} ${GEN_C_OUT} ${CAULK_SRC_DIR}/caulk.cpp)
add_dependencies(${CAULK_LIB_NAME} ${GEN_NAME})
set_target_properties(${CAULK_LIB_NAME} PROPERTIES LINKER_LANGUAGE C)
target_compile_definitions(${CAULK_LIB_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS=1)
target_include_directories(
    ${CAULK_LIB_NAME}
    PUBLIC ${GEN_OUT_DIR}
    PRIVATE ${SDK_INCLUDE_DIR}/steam ${SDK_INCLUDE_DIR})

set(DYLIB_ROOT ${SDK_ROOT}/redistributable_bin)
if(WIN32)
    if(${CMAKE_SIZEOF_VOID_P} STREQUAL 4) # 32 bitches...
        set(CAULK_LINK_AGAINST
            ${DYLIB_ROOT}/steam_api.lib
            CACHE INTERNAL "")
        set(CAULK_COPY_DLL
            ${DYLIB_ROOT}/steam_api.dll
            CACHE INTERNAL "")
    else()
        set(CAULK_LINK_AGAINST
            ${DYLIB_ROOT}/win64/steam_api64.lib
            CACHE INTERNAL "")
        set(CAULK_COPY_DLL
            ${DYLIB_ROOT}/win64/steam_api64.dll
            CACHE INTERNAL "")
    endif()
else()
    if(LINUX)
        if(${CMAKE_SIZEOF_VOID_P} STREQUAL 4) # 32 bitches...
            set(STEAMWORKS_DLL_PATH ${DYLIB_ROOT}/linux32/libsteam_api.so)
        else()
            set(STEAMWORKS_DLL_PATH ${DYLIB_ROOT}/linux64/libsteam_api.so)
        endif()
    else()
        set(STEAMWORKS_DLL_PATH ${DYLIB_ROOT}/osx/libsteam_api.dylib)
    endif()

    set(CAULK_COPY_DLL
        ${STEAMWORKS_DLL_PATH}
        CACHE INTERNAL "")
    set(CAULK_LINK_AGAINST
        ${CAULK_COPY_DLL}
        CACHE INTERNAL "")
endif()
target_link_libraries(${CAULK_LIB_NAME} PUBLIC ${CAULK_LINK_AGAINST})

function(caulk_register TGT)
    # Stupid dwarf trick for dynamically linked SteamAPI to work...
    if(UNIX)
        set_property(TARGET ${TGT} PROPERTY LINK_FLAGS "-Wl,-rpath='$ORIGIN',-rpath-link='$ORIGIN'")
    endif()

    if(DEFINED CAULK_COPY_DLL)
        add_custom_command(
            TARGET ${TGT}
            PRE_BUILD
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
    target_link_libraries(${TEST_NAME} PRIVATE ${CAULK_LIB_NAME})
endif()
