cmake_minimum_required(VERSION 3.15)

project(WordNetDiyAi LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(WORDNET_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT DEFINED DEFAULTPATH)
    set(DEFAULTPATH "${WORDNET_ROOT}/dict" CACHE STRING "Default path for WordNet data files")
endif()
if(NOT EXISTS "${DEFAULTPATH}")
    set(DEFAULTPATH "${WORDNET_ROOT}/dict" CACHE STRING "Default path for WordNet data files" FORCE)
endif()
if(PROJECT_NAME STREQUAL "WordNetDiyAi")
    add_compile_definitions(DEFAULTPATH="${DEFAULTPATH}")
endif()

if(NOT TARGET WN)
    add_subdirectory("${WORDNET_ROOT}/lib" "${CMAKE_CURRENT_BINARY_DIR}/lib")
    target_include_directories(WN PUBLIC
        "${WORDNET_ROOT}"
        "${WORDNET_ROOT}/include"
    )
endif()
if(PROJECT_NAME STREQUAL "WordNetDiyAi" AND TARGET WN)
    target_compile_definitions(WN PUBLIC DEFAULTPATH="${DEFAULTPATH}")
endif()

find_path(RAPIDJSON_INCLUDE_DIR rapidjson/document.h)
if(NOT RAPIDJSON_INCLUDE_DIR)
    include(FetchContent)
    FetchContent_Declare(
        rapidjson
        GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
        GIT_TAG v1.1.0
    )
    FetchContent_MakeAvailable(rapidjson)
    set(RAPIDJSON_INCLUDE_DIR "${rapidjson_SOURCE_DIR}/include")
endif()

add_executable(wn-meaning meaning.c)
target_link_libraries(wn-meaning WN)
target_include_directories(wn-meaning PRIVATE
    "${WORDNET_ROOT}"
    "${WORDNET_ROOT}/include"
)

add_executable(wn-chat chat.c json_extract.cpp)
target_link_libraries(wn-chat WN)
target_include_directories(wn-chat PRIVATE
    "${WORDNET_ROOT}"
    "${WORDNET_ROOT}/include"
    "${RAPIDJSON_INCLUDE_DIR}"
)

install(TARGETS wn-meaning DESTINATION bin)
install(TARGETS wn-chat DESTINATION bin)
