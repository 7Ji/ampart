set (LIB_VERSION_MAJOR 0)
set (LIB_VERSION_MINOR 0)
set (LIB_VERSION_PATCH 1)
set (LIB_VERSION "${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${LIB_VERSION_PATCH}")

file (GLOB
    LIB_SRC
    CONFIGURE_DEPENDS
        *.c
)

if ("${LIBAMPART_VERSION}" STREQUAL "")
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
    set (LIBAMPART_VERSION "${AMPART_VERSION}")
else ()
    message("-- Version defined: ${LIBAMPART_VERSION}")
endif ()

add_library (libampart
    ${LIB_SRC}
)

target_include_directories(libampart
    PUBLIC
        "../include"
        "../include/priv"
)

target_compile_definitions(libampart
    PUBLIC
        LIBAMPART_VERSION=\"${LIBAMPART_VERSION}\"
)

target_compile_options(libampart
    PUBLIC
        -Wall
        -Wextra
)

set_target_properties(libampart
    PROPERTIES
        OUTPUT_NAME "ampart"
)

target_link_libraries(libampart
    z
)
