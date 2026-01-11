cmake_minimum_required(VERSION 3.15)

# Library sources
set(LIBWN_SOURCES
    binsrch.c
    morph.c
    search.c
    wnglobal.c
    wnhelp.c
    wnrtl.c
    wnutil.c
)

# Create static library
add_library(WN STATIC ${LIBWN_SOURCES})

# Include directories for the library
target_include_directories(WN PUBLIC 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Install the library
install(TARGETS WN 
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)
