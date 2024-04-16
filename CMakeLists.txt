cmake_minimum_required(VERSION 3.5)

project(ampart C)

if ("${VERSION}" STREQUAL "")
    execute_process(
        COMMAND bash scripts/build-only-version.sh
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE VERSION
        ERROR_QUIET)
    message("-- Version generated: ${VERSION}")
else()
    message("-- Version defined: ${VERSION}")
endif()

set_source_files_properties(src/version.c
    PROPERTIES COMPILE_DEFINITIONS VERSION=\"${VERSION}\")

add_executable(ampart
    src/cli.c
    src/dtb.c
    src/dts.c
    src/ept.c
    src/gzip.c
    src/io.c
    src/main.c
    src/parg.c
    src/size.c
    src/stringblock.c
    src/util.c
    src/version.c)

target_link_libraries(ampart
    z)

target_include_directories(ampart PRIVATE
    "include")
 
target_compile_options(ampart PRIVATE
    -Wall
    -Wextra)
