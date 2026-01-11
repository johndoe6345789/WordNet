cmake_minimum_required(VERSION 3.15)

# Build the wn command-line tool (no Tcl/Tk required)
add_executable(wn wn.c)
target_link_libraries(wn WN)
target_include_directories(wn PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Install wn executable
install(TARGETS wn DESTINATION bin)

# Install wnb script
install(PROGRAMS wnb DESTINATION bin)

# Optionally build wishwn if Tcl/Tk is available
find_package(TCL QUIET)
find_package(Tk QUIET)

if(TCL_FOUND AND TK_FOUND)
    message(STATUS "Tcl/Tk found - building wishwn")
    
    add_executable(wishwn tkAppInit.c stubs.c)
    target_link_libraries(wishwn WN ${TCL_LIBRARY} ${TK_LIBRARY})
    target_include_directories(wishwn PRIVATE 
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${TCL_INCLUDE_PATH}
        ${TK_INCLUDE_PATH}
    )
    
    # Install wishwn executable
    install(TARGETS wishwn DESTINATION bin)
else()
    message(STATUS "Tcl/Tk not found - skipping wishwn (GUI browser)")
    message(STATUS "The command-line tool 'wn' will still be built")
endif()
