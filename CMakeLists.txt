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
if(NOT EXISTS ${STEAMWORKS_SDK_ZIP})
    message(FATAL_ERROR "You forgot to specify a path to Steamworks SDK .zip! Review the project's README")
endif()

set(raw_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/steamworks)
file(ARCHIVE_EXTRACT INPUT ${STEAMWORKS_SDK_ZIP} DESTINATION ${raw_SOURCE_DIR})

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIB_NAME ${PROJECT_NAME})
set(RAW_ROOT ${raw_SOURCE_DIR}/sdk)
set(RAW_INCLUDE_DIR ${RAW_ROOT}/public)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(GEN_NAME ${LIB_NAME}GenBindings)
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

add_library(${LIB_NAME} ${SRC_DIR}/caulk.h ${SRC_DIR}/caulk.cpp ${GEN_H_OUT} ${GEN_C_OUT})
add_dependencies(${LIB_NAME} ${GEN_NAME})
set_target_properties(${LIB_NAME} PROPERTIES LINKER_LANGUAGE C)
target_compile_definitions(${LIB_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS=1)
target_include_directories(
    ${LIB_NAME}
    PUBLIC ${SRC_DIR} ${RAW_INCLUDE_DIR} ${GEN_OUT_DIR}
    PRIVATE ${RAW_INCLUDE_DIR}/steam)

set(TEST_NAME ${LIB_NAME}Test)
add_executable(${TEST_NAME} ${SRC_DIR}/test.c)
set_target_properties(${TEST_NAME} PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(${TEST_NAME} ${LIB_NAME})

add_custom_command(
    TARGET ${TEST_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/steam_appid.txt $<TARGET_FILE_DIR:${TEST_NAME}>
    COMMAND_EXPAND_LISTS)

set(DYLIB_ROOT ${RAW_ROOT}/redistributable_bin)
if(WIN32)
    set(STEAM_LIB "win64/steam_api64")
    if(${CMAKE_SIZEOF_VOID_P} STREQUAL 4) # 32 bitches...
        set(STEAM_LIB "steam_api")
    endif()
    set(CAULK_COPY_DLL ${DYLIB_ROOT}/${STEAM_LIB}.dll)
    target_link_libraries(${LIB_NAME} PUBLIC ${DYLIB_ROOT}/${STEAM_LIB}.lib)
elseif(LINUX)
    set(STEAM_LIB "linux64")
    if(${CMAKE_SIZEOF_VOID_P} STREQUAL 4) # 32 bitches...
        set(STEAM_LIB "linux32")
    endif()
    set(CAULK_COPY_DLL ${DYLIB_ROOT}/${STEAM_LIB}/libsteam_api.so)
    target_link_libraries(${LIB_NAME} PUBLIC ${CAULK_COPY_DLL})
else()
    target_link_libraries(${LIB_NAME} PUBLIC ${DYLIB_ROOT}/osx/libsteam_api.dylib)
endif()

function(caulk_register TGT)
    target_link_libraries(${TGT} ${LIB_NAME})

    if(DEFINED CAULK_COPY_DLL)
        add_custom_command(
            TARGET ${TGT}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} ARGS -E copy ${CAULK_COPY_DLL} $<TARGET_FILE_DIR:${TGT}>)
    endif()

    set(STEAM_APPID ${CMAKE_CURRENT_SOURCE_DIR}/steam_appid.txt)
    if(NOT EXISTS ${STEAM_APPID})
        message(FATAL_ERROR "You forgot to add a steam_appid.txt! Just write \"480\" inside it.")
    endif()

    add_custom_command(
        TARGET ${TGT}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} ARGS -E copy ${STEAM_APPID} $<TARGET_FILE_DIR:${TGT}>)
endfunction()

caulk_register(${TEST_NAME})
