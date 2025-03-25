cmake_minimum_required (VERSION 3.10)

project (ampart C)

set (CMAKE_C_STANDARD 99)

add_subdirectory (lib)

if ("${AMPART_VERSION}" STREQUAL "")
    execute_process(
        COMMAND
            bash scripts/build-only-version.sh
        WORKING_DIRECTORY
            "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE AMPART_VERSION
        ERROR_QUIET)
    message("-- Version generated: ${AMPART_VERSION}")
else ()
    message("-- Version defined: ${AMPART_VERSION}")
endif ()

add_executable(ampart
    ampart.c
)

target_link_libraries(ampart
    libampart
)

target_compile_definitions(libampart
    PUBLIC
        AMPART_VERSION=\"${AMPART_VERSION}\"
)

# target_include_directories(ampart PRIVATE
#     "include"
#     "include/priv"
# )

# target_compile_options(ampart PRIVATE
#     -Wall
#     -Wextra
# )
