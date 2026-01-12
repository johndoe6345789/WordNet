cmake_minimum_required(VERSION 3.15)
project(WordNet VERSION 3.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Export compile_commands.json for tooling by default.
option(WORDNET_EXPORT_COMPILE_COMMANDS "Export compile_commands.json for tooling" ON)
if(WORDNET_EXPORT_COMPILE_COMMANDS)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# Default installation prefix
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local/WordNet-3.0" CACHE PATH "Default install path" FORCE)
endif()

# Define DEFAULTPATH for dictionary files
set(DEFAULTPATH "${CMAKE_INSTALL_PREFIX}/dict" CACHE STRING "Default path for WordNet data files")

# Include directories
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

# Add subdirectories
add_subdirectory(lib)
if(TARGET WN)
    target_compile_definitions(WN PUBLIC DEFAULTPATH="${DEFAULTPATH}")
endif()
add_subdirectory(src)

# Optional DIY AI demo tooling
option(WORDNET_BUILD_DIY_AI "Build DIY AI demo tools" ON)
if(WORDNET_BUILD_DIY_AI)
    add_subdirectory(diy-ai)
endif()

# Linting helpers (clang-tidy and cppcheck) if available.
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
find_program(CPPCHECK_EXE NAMES cppcheck)

function(wordnet_collect_sources out_var target)
    if(TARGET ${target})
        get_target_property(_srcs ${target} SOURCES)
        get_target_property(_src_dir ${target} SOURCE_DIR)
        set(_abs "")
        foreach(_src IN LISTS _srcs)
            if(IS_ABSOLUTE "${_src}")
                list(APPEND _abs "${_src}")
            else()
                list(APPEND _abs "${_src_dir}/${_src}")
            endif()
        endforeach()
        set(${out_var} ${_abs} PARENT_SCOPE)
    endif()
endfunction()

if(CLANG_TIDY_EXE OR CPPCHECK_EXE)
    set(WORDNET_LINT_SOURCES "")
    wordnet_collect_sources(WN_SOURCES WN)
    wordnet_collect_sources(WN_EXE_SOURCES wn)
    wordnet_collect_sources(WISHWN_SOURCES wishwn)
    list(APPEND WORDNET_LINT_SOURCES ${WN_SOURCES} ${WN_EXE_SOURCES} ${WISHWN_SOURCES})
    list(REMOVE_DUPLICATES WORDNET_LINT_SOURCES)
endif()

if(CLANG_TIDY_EXE AND WORDNET_LINT_SOURCES)
    add_custom_target(lint-clang-tidy
        COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} ${WORDNET_LINT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy")
endif()

if(CPPCHECK_EXE)
    add_custom_target(lint-cppcheck
        COMMAND ${CPPCHECK_EXE}
            --project=${CMAKE_BINARY_DIR}/compile_commands.json
            --enable=warning,style,performance,portability
            --std=c99
            --inline-suppr
            --suppress=missingIncludeSystem
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck")
endif()

if(CLANG_TIDY_EXE OR CPPCHECK_EXE)
    add_custom_target(lint)
    if(CLANG_TIDY_EXE AND WORDNET_LINT_SOURCES)
        add_dependencies(lint lint-clang-tidy)
    endif()
    if(CPPCHECK_EXE)
        add_dependencies(lint lint-cppcheck)
    endif()
endif()

# Install dictionary files
install(DIRECTORY dict/ 
        DESTINATION dict
        FILES_MATCHING 
        PATTERN "*.adj"
        PATTERN "*.adv"
        PATTERN "*.noun"
        PATTERN "*.verb"
        PATTERN "*.exc"
        PATTERN "*.vrb"
        PATTERN "cntlist*"
        PATTERN "index.*"
        PATTERN "data.*"
        PATTERN "sent*"
        PATTERN "Makefile*" EXCLUDE)

# Install headers
install(FILES include/wn.h include/wngrind.h DESTINATION include)

# Install documentation
install(DIRECTORY doc/html/ DESTINATION doc/html FILES_MATCHING PATTERN "*.html" PATTERN "*.gif" PATTERN "*.css" PATTERN "Makefile*" EXCLUDE)
install(DIRECTORY doc/pdf/ DESTINATION doc/pdf FILES_MATCHING PATTERN "*.pdf" PATTERN "Makefile*" EXCLUDE)
install(DIRECTORY doc/ps/ DESTINATION doc/ps FILES_MATCHING PATTERN "*.ps" PATTERN "Makefile*" EXCLUDE)
install(DIRECTORY doc/man/ DESTINATION man/man1 FILES_MATCHING PATTERN "*.[1-9]" PATTERN "Makefile*" EXCLUDE)

# Print installation info
message(STATUS "WordNet ${PROJECT_VERSION} configured")
message(STATUS "Installation directory: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Dictionary path: ${DEFAULTPATH}")
