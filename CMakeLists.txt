cmake_minimum_required(VERSION 3.28.0 FATAL_ERROR)

project(caulk VERSION 0.1.0
    DESCRIPTION "THE Steamworks API wrapper for use with plain C"
    LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG 0.11.1)
FetchContent_MakeAvailable(yyjson)

set(STEAMWORKS_SDK_ZIP ${CMAKE_SOURCE_DIR}/steamworks_sdk_163.zip
    CACHE STRING "Path to Steamworks SDK (zip)")
if(NOT EXISTS ${STEAMWORKS_SDK_ZIP} AND NOT CAULK_GENERATOR_ONLY)
    message(FATAL_ERROR
        "You forgot to specify a path to Steamworks SDK .zip! "
        "Or maybe it couldn't be found. Check the README either way")
endif()

set(STEAM_APPID ${CMAKE_SOURCE_DIR}/steam_appid.txt
    CACHE STRING "Path to steam_appid.txt")
if(NOT EXISTS ${STEAM_APPID} AND NOT CAULK_GENERATOR_ONLY)
    message(FATAL_ERROR
        "You forgot to add a steam_appid.txt! Just write \"480\" inside it.")
endif()

set(SDK_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/steamworks)
if(NOT CAULK_GENERATOR_ONLY)
    file(ARCHIVE_EXTRACT INPUT ${STEAMWORKS_SDK_ZIP} DESTINATION ${SDK_SOURCE_DIR})
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SDK_ROOT ${SDK_SOURCE_DIR}/sdk)
set(SDK_INCLUDE_DIR ${SDK_ROOT}/public)
set(CAULK_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(NOT CAULK_PREBUILT_GENERATOR)
    add_executable(caulkGlueGenerator ${CAULK_SRC_DIR}/generator.c)
    target_link_libraries(caulkGlueGenerator PRIVATE yyjson)
endif()

set(GEN_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(GEN_OUT_DIR_PUB ${CMAKE_CURRENT_BINARY_DIR}/pub)
file(MAKE_DIRECTORY ${GEN_OUT_DIR_PUB})

set(GEN_API_OUT ${GEN_OUT_DIR}/__api.h)
set(GEN_H_OUT ${GEN_OUT_DIR_PUB}/caulk.h)
set(GEN_C_OUT ${GEN_OUT_DIR}/__gen.cpp)

set(STEAM_API_JSON ${SDK_INCLUDE_DIR}/steam/steam_api.json)

add_custom_command(
    OUTPUT ${GEN_H_OUT} ${GEN_C_OUT}
    COMMAND $<IF:$<BOOL:${CAULK_PREBUILT_GENERATOR}>,${CAULK_PREBUILT_GENERATOR},$<TARGET_FILE:caulkGlueGenerator>>
    ${GEN_H_OUT} ${GEN_C_OUT} ${GEN_API_OUT} ${STEAM_API_JSON}
    VERBATIM)

add_library(caulk ${GEN_H_OUT} ${GEN_C_OUT} ${CAULK_SRC_DIR}/caulk.cpp)
set_target_properties(caulk PROPERTIES LINKER_LANGUAGE C)
target_compile_definitions(caulk PRIVATE _CRT_SECURE_NO_WARNINGS=1)
target_include_directories(caulk
    PUBLIC ${GEN_OUT_DIR_PUB}
    INTERFACE ${GEN_OUT_DIR}
    PRIVATE ${SDK_INCLUDE_DIR}/steam ${SDK_INCLUDE_DIR})

set(DYLIB_ROOT ${SDK_ROOT}/redistributable_bin)
if(WIN32)
    if(${CMAKE_SIZEOF_VOID_P} STREQUAL 4) # 32 bitches...
        set(CAULK_LINK_AGAINST ${DYLIB_ROOT}/steam_api.lib
            CACHE INTERNAL "")
        set(CAULK_COPY_DLL ${DYLIB_ROOT}/steam_api.dll
            CACHE INTERNAL "")
    else()
        set(CAULK_LINK_AGAINST ${DYLIB_ROOT}/win64/steam_api64.lib
            CACHE INTERNAL "")
        set(CAULK_COPY_DLL ${DYLIB_ROOT}/win64/steam_api64.dll
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

    set(CAULK_COPY_DLL ${STEAMWORKS_DLL_PATH}
        CACHE INTERNAL "")
    set(CAULK_LINK_AGAINST ${CAULK_COPY_DLL}
        CACHE INTERNAL "")
endif()
target_link_libraries(caulk PUBLIC ${CAULK_LINK_AGAINST})

function(caulk_populate TGT)
    if(UNIX) # Stupid dwarf trick for dynamically linked SteamAPI to work...
        target_link_options(${TGT} PRIVATE "LINKER:-rpath,./")
    endif()

    if(DEFINED CAULK_COPY_DLL)
        add_custom_command(
            TARGET ${TGT} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
            -E copy ${CAULK_COPY_DLL} $<TARGET_FILE_DIR:${TGT}>
            VERBATIM)
    endif()

    add_custom_command(
        TARGET ${TGT} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        -E copy ${STEAM_APPID} $<TARGET_FILE_DIR:${TGT}>
        VERBATIM)
endfunction()

option(CAULK_BUILD_TEST "Build the caulkTest executable?" ON)
if(CAULK_BUILD_TEST AND NOT CAULK_GENERATOR_ONLY)
    add_executable(caulkTest ${CAULK_SRC_DIR}/test.c)
    set_target_properties(caulkTest PROPERTIES LINKER_LANGUAGE C)
    target_link_libraries(caulkTest PRIVATE caulk)
    caulk_populate(caulkTest)
endif()
