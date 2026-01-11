cmake_minimum_required(VERSION 3.15)
project(WordNet VERSION 3.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default installation prefix
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local/WordNet-3.0" CACHE PATH "Default install path" FORCE)
endif()

# Define DEFAULTPATH for dictionary files
set(DEFAULTPATH "${CMAKE_INSTALL_PREFIX}/dict" CACHE STRING "Default path for WordNet data files")
add_compile_definitions(DEFAULTPATH="${DEFAULTPATH}")

# Include directories
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

# Add subdirectories
add_subdirectory(lib)
add_subdirectory(src)

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
