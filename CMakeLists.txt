cmake_minimum_required(VERSION 3.26.0 FATAL_ERROR)

project(
    caulk
    VERSION 0.1.0
    DESCRIPTION "THE Steamworks API wrapper for use with CMake."
    LANGUAGES C)

include(FetchContent)

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

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(RAW_ROOT ${raw_SOURCE_DIR}/sdk)
set(RAW_INCLUDE_DIR ${RAW_ROOT}/public)

add_library(${PROJECT_NAME} ${SRC_DIR}/caulk.h)
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)
target_include_directories(
    ${PROJECT_NAME}
    PUBLIC ${SRC_DIR}
    PRIVATE ${RAW_INCLUDE_DIR})

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
