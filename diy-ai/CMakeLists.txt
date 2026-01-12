cmake_minimum_required(VERSION 3.15)

add_executable(wn-meaning meaning.c)
target_link_libraries(wn-meaning WN)
target_include_directories(wn-meaning PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

install(TARGETS wn-meaning DESTINATION bin)
